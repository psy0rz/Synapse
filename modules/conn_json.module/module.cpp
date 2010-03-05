#include "cnetman.h"
#include "synapse.h"

#include "synapse_json.h"
#define MAX_MESSAGE 2048


/**
	-New connections are handled in a session with user anonymous
	-Received network message are send to the core.
	-Received core messages are send to the network.

	-The session should do a core_Login


*/



int networkSessionId=0;

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	//max number of parallel module threads
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=100;
	out.send();

	//The default session will be used to receive broadcasts that need to be transported via json.
	//Make sure we only process 1 message at a time, so they stay in order.
	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=1;
	out.send();

	//We use a ANONYMOUS session that run the threads for network connection handeling.
	//new connections will be anonymous and have to login with core_Login.
	out.clear();
	out.event="core_NewSession";
	out["maxThreads"]=1;
	out.send();

	//make sure the anonymous session has the right to receive the basic stuff:
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="conn_json_Connect";
	out["recvGroup"]="anonymous";
	out.send();

	out["event"]="conn_json_Listen";
	out.send();

	out["event"]="conn_json_Disconnect";
	out.send();
 
	out["event"]="conn_json_Close";
	out.send();


	//register a special handler without specified event
	//this will receive all events that are not handled elsewhere in this module.
	out.clear();
	out.event="core_Register";
	out["handler"]="all";
	out.send();

}



// We extent the Cnet class with our own network handlers.

// As soon as something with a network connection 'happens', these handlers will be called.
// This stuff basically runs as anonymous, until a user uses core_login to change the user.
class CnetModule : public Cnet
{
	/** Connection 'id' is trying to connect to host:port
	* Sends: net_Connecting
	*/
 	void connecting(int id, const string &host, int port)
	{
		Cmsg out;
		out.dst=id;
		out.event="conn_Connecting";
		out["host"]=host;
		out["port"]=port;
		out["type"]="json";
		out.send();
	}

	/** Client connection 'id' is established.
	*/
 	void connected_client(int id, const string &host, int port)
	{
		Cmsg out;
		out.dst=id;
		out.event="conn_Connected";
		out["type"]="json";
		out.send();
	}

	/** Server connection 'id' is established.
	*/
 	void connected_server(int id, const string &host, int port)
	{
		//someone is connecting us..
	}


	/** Connection 'id' has received new data.
	*/
	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		Cmsg out;
	
		if (bytesTransferred > MAX_MESSAGE)
		{
			WARNING("Json message on id " << id << " is " << bytesTransferred << " bytes. (max=" << MAX_MESSAGE << ")");
			return ;
		}

		if (json2Cmsg(readBuffer, out))
		{
			out.send();
		}
		else
		{
			WARNING("Error while parsing incomming json message on id " << id);
		}
	}

	/** Connection 'id' is disconnected, or a connect-attempt has failed.
	* Sends: conn_Disconnected
	*/
 	void disconnected(int id, const boost::system::error_code& ec)
	{
		Cmsg out;
		out.dst=id;
		out.event="conn_Disconnected";
		out["reason"]=ec.message();
		out["type"]="json";
		out.send();
	}
};

CnetMan<CnetModule> net;




SYNAPSE_REGISTER(module_SessionStart)
{

	//first new session, this is the network thread session
	if (!networkSessionId)
	{
		networkSessionId=msg.dst;

		//tell the rest of the world we are ready for duty
		Cmsg out;
		out.event="core_Ready";
		out.src=networkSessionId;
		out.send();

		return;
	}

	//new session that was started to run the acceptor for a port:
	if (msg.isSet("port"))
	{
		//keep accepting until shutdown or some other error
		while(net.runAccept(msg["port"], msg.dst));
		
		//we're done, delete our session
		Cmsg out;
		out.event="core_DelSession";
		out.src=msg.dst;
		out.send();
	}

}

/** Client-only: Create a new json connection, connects to host:port for session src
 */
SYNAPSE_REGISTER(conn_json_Connect)
{
	if (msg.dst==networkSessionId)
		net.runConnect(msg.src, msg["host"], msg["port"]);
	else
		ERROR("Send to the wrong session id");
}

/** Server only: Creates a new server and listens specified port
 */
SYNAPSE_REGISTER(conn_json_Listen)
{
	if (msg.dst==networkSessionId)
	{
 		//start a new anonymous acceptors to accept new incoming connections and handle the incomming data
		Cmsg out;
 		out.event="core_NewSession";
 		out["username"]="anonymous";
 		out["password"]="anonymous";
 		out["port"]=msg["port"];
 		out.send();

		net.runListen(msg["port"]);
	}
	else
		ERROR("Send to the wrong session id");

	
}


/** Server only: Stop listening on a port
 */
SYNAPSE_REGISTER(conn_json_Close)
{
	if (msg.dst==networkSessionId)
		net.doClose(msg["port"]);
	else
		ERROR("Send to the wrong session id");
}


	

/** Disconnections the connection related to src 
 */
SYNAPSE_REGISTER(conn_json_Disconnect)
{
	net.doDisconnect(msg.src);
}


/** When a session ends, make sure the corresponding network connection is disconnected as well.
 *
 */
SYNAPSE_REGISTER(module_SessionEnded)
{
	net.doDisconnect(msg["session"]);
}

/** Called when synapse whats the module to shutdown completely
 * This makes sure that all ports and network connections are closed, so there wont be any 'hanging' threads left.
 * If you care about data-ordering, send this to session-id that sended you the net_Connected.
 */
SYNAPSE_REGISTER(module_Shutdown)
{
	//let the net module shut down to fix the rest
	net.doShutdown();
}





/** This handler is called for all events that:
 * -dont have a specific handler, 
 * -are send to broadcast or to a session we manage.
 */
SYNAPSE_HANDLER(all)
{
	string jsonStr;
	Cmsg2json(msg, jsonStr);
	
	//send to corresponding network connection
	INFO("json: " << jsonStr);
}




#include "cnetman.h"
#include "synapse.h"

#define MAX_MESSAGE 2048




int moduleSessionId=0;

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	moduleSessionId=msg.dst;

	//change module settings. 
	//especially broadcastMulti is important for our "all"-handler
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=100;
	out["broadcastMulti"]=1;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=100;
	out.send();

	//register a special handler without specified event
	//this will receive all events that are not handled elsewhere in this module.
	out.clear();
	out.event="core_Register";
	out["handler"]="all";
	out.send();

	//tell the rest of the world we are ready for duty
	//Cmsg out;
	out.event="core_Ready";
	out.send();

}



// We extent the Cnet class with our own network handlers.

// As soon as something with a network connection 'happens', these handlers will be called.
// This stuff basically runs as anonymous, until a user uses core_login to change the user.
class CnetModule : public Cnet
{


	/** Server connection 'id' is established.
	*/
 	void connected_server(int id, const string &host, int port)
	{
		//someone is connecting us, create a new session initial session for this connection
		Cmsg out;
		out.event="core_NewSession";
		out["synapse_cookie"]=id;
		out["username"]="anonymous";
		out["password"]="anonymous";
		out.send();

	}


	/** Connection 'id' has received new data.
	*/
	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		if (bytesTransferred > MAX_MESSAGE)
		{
			WARNING("Json message on id " << id << " is " << bytesTransferred << " bytes. (max=" << MAX_MESSAGE << ")");
			return ;
		}

		Cmsg out;
		string dataStr(boost::asio::buffer_cast<const char*>(readBuffer.data()), readBuffer.size());
		dataStr.resize(dataStr.find(delimiter)+delimiter.length());

		if (out.fromJson(dataStr))
		{
			out.send(id);
		}
		else
		{
			WARNING("Error while parsing incoming json message on id " << id);
		}

		readBuffer.consume(dataStr.length());
	}

//	/** Connection 'id' is disconnected, or a connect-attempt has failed.
//	* Sends: conn_Disconnected
//	*/
// 	void disconnected(int id, const boost::system::error_code& ec)
//	{
//		Cmsg out;
//		out.dst=id;
//		out.event="conn_Disconnected";
//		out["reason"]=ec.message();
//		out["type"]="json";
//		out.send();
//	}
};

CnetMan<CnetModule> net;


void writeMessage(int id, Cmsg & msg)
{
	string msgStr;
	msg.toJson(msgStr);
	msgStr+="\n";
	net.doWrite(id,msgStr);
}






/** Server only: Creates a new server and listens specified port
 */
SYNAPSE_REGISTER(conn_json_Listen)
{
	if (msg.dst==moduleSessionId)
	{
		//starts a new thread to accept and handle the incomming connection.
		Cmsg out;
		
		//fire off acceptor threads
		out.clear();
		out.dst=msg.dst;
		out.event="conn_json_Accept";
		out["port"]=msg["port"];
		for (int i=0; i<10; i++)
	 		out.send();

		//become the listening thread
		net.runListen(msg["port"]);

	}
	else
		ERROR("Send to the wrong session id");
}


/** Server only: Stop listening on a port
 */
SYNAPSE_REGISTER(conn_json_Close)
{
	net.doClose(msg["port"]);
}

/** Runs an acceptor thread (only used internally)
 */
SYNAPSE_REGISTER(conn_json_Accept)
{
	if (msg.dst==moduleSessionId)
	{
		//keep accepting until shutdown or some other error
		while(net.runAccept(msg["port"], 0));
	}

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
	if (cookie)
		writeMessage(cookie,msg);
}





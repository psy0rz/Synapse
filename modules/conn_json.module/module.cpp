#include "cnetman.h"
#include "synapse.h"

#define MAX_MESSAGE 2048

using namespace boost;
using namespace std;

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

void writeMessage(int id, Cmsg & msg);


// We extent the Cnet class with our own network handlers.

// As soon as something with a network connection 'happens', these handlers will be called.
// This stuff basically runs as anonymous, until a user uses core_login to change the user.
class CnetModule : public synapse::Cnet
{


	/** Server connection 'id' is established.
	*/
 	void connected_server(int id, const string &host, int port, int local_port)
	{
		Cmsg out;

		//fireoff a new acceptor thread, to accept future connections.
		out.clear();
		out.dst=moduleSessionId;
		out.event="conn_json_Accept";
		out["port"]=local_port;
 		out.send();


 		//someone has connecting us, create a new session initial session for this connection
		out.clear();
		out.dst=0;
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
			stringstream s;
			s << "Json message on id " << id << " is " << bytesTransferred << " bytes. (max=" << MAX_MESSAGE << ")";
			Cmsg errMsg;
			errMsg["description"]=s.str();
			errMsg.event="error";
			writeMessage(id,errMsg);
			return ;
		}

		Cmsg out;
		string dataStr(boost::asio::buffer_cast<const char*>(readBuffer.data()), readBuffer.size());
		dataStr.resize(dataStr.find(delimiter)+delimiter.length());

		try
		{
			out.fromJson(dataStr);

			//if its requesting a new session, make sure the correct cookie is sended along:
			if (out.event=="core_NewSession")
			{
				out["synapse_cookie"]=id;
			}
			//send it and handle send errors
			string error=out.send(id);
			if (error!="")
			{
				Cmsg errMsg;
				errMsg["description"]=error;
				errMsg.event="error";
				writeMessage(id,errMsg);
			}
		}
		catch(std::exception& e)
		{
			Cmsg errMsg;
			errMsg["description"]="Error while parsing incoming json message: "+string(e.what());
			errMsg.event="error";
			writeMessage(id,errMsg);
		}

		readBuffer.consume(dataStr.length());
	}

	/** Connection 'id' is disconnected, or a connect-attempt has failed.
	* Sends: conn_Disconnected
	*/
 	void disconnected(int id, const boost::system::error_code& ec)
	{
		Cmsg out;
		out.event="core_DelCookieSessions";
		out["cookie"]=id;
		out.send();
	}
};


synapse::CnetMan<CnetModule> net;

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
		
		//fire off acceptor thread
		out.clear();
		out.dst=msg.dst;
		out.event="conn_json_Accept";
		out["port"]=msg["port"];
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
		net.runAccept(msg["port"], 0);
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
	msg.dst=dst;
	if (cookie)
		writeMessage(cookie,msg);
}





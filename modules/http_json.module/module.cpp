#include "cnetman.h"
#include "synapse.h"
#include <boost/regex.hpp>

#include "synapse_json.h"
#define MAX_MESSAGE 2048


/**
	THIS MODULE IS UNFINISHED - DO NOT USE

	Sessions are identified with a cookie.
	The java script in the browser uses XMLhttprequest POST to post events. 
	The browser uses XMLhttprequest GET to receive events.
	We try to use multipart/x-mixed-replace to use peristent connections.


*/



int networkSessionId=0;
int netIdCounter=0;

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	//max number of parallel module threads
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=100;
	out.send();

	//The default session will be used to receive broadcasts that need to be transported via json.
	//Make sure we only process 1 message at a time (1 thread), so they stay in order.
	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=1;
	out.send();

	//Create a session for incomming connection handling
	out.clear();
	out.event="core_NewSession";
	out["maxThreads"]=20;
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
	void init(int id)
	{
		delimiter="\r\n\r\n";
	}

	/** Sombody connected us
	*/
 	void connected_server(int id, const string &host, int port)
	{

	}


	/** Connection 'id' has received new data.
	*/
	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		/* Parse the http request
		*/

		string s(boost::asio::buffer_cast<const char*>(readBuffer.data()), bytesTransferred);
		INFO("Got http shizzle: \n" << s);
		doDisconnect();	
/*		

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
		}*/
	}

	/** Connection 'id' is disconnected, or a connect-attempt has failed.
	* Sends: conn_Disconnected
	*/
 	void disconnected(int id, const boost::system::error_code& ec)
	{
	}
};

CnetMan<CnetModule> net;


SYNAPSE_REGISTER(module_SessionStart)
{

	//first new session, this is the network threads session
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

}

/** Creates a new server and listens specified port
 */
SYNAPSE_REGISTER(http_json_Listen)
{
	if (msg.dst==networkSessionId)
	{
		//starts a new thread to accept and handle the incomming connection.
		Cmsg out;
		out.dst=networkSessionId;
 		out.event="http_json_Accept";
 		out["port"]=msg["port"];
	
		//start 10 accepting threads (e.g. max 10 connections)
		for (int i=0; i<10; i++)
	 		out.send();

		net.runListen(msg["port"]);
	}
	else
		ERROR("Send to the wrong session id");
	
}


/** Runs an acceptor thread (only used internally)
 */
SYNAPSE_REGISTER(http_json_Accept)
{
		//keep accepting until shutdown or some other error
		while(net.runAccept(msg["port"], 0));
}

/** Stop listening on a port
 */
SYNAPSE_REGISTER(http_json_Close)
{
	net.doClose(msg["port"]);
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




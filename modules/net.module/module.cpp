
/** Generic line based network client module.
 * Use this module as an example to build your own network cabable modules. (like lirc)
 * Connections are related to the session id that sended the message.
 * So:
 * -Every session has only 1 unique connection. If you want another connecting, start another session frist.
 * -Other sessions (thus users) cannot influence the conneciton.
 * -Received data will be send only to the session.
 *
 * The framework gives you the freedom to implement network-modules anyway you like: You could also decide to
 * automaticly create new sessions in this module, when someone wants to make a connection.
 *
 */



#include <string>

#include "cnet.h"
#include "synapse.h"

/** module_Init - called first, set up basic stuff here
 */
SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	//max number of parallel threads
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=3000;
	out.send();

	//The default session will be used to call the run-functions.
	//Every new connection needs a seperate thread.
	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=1000;
	out.send();

	//Make a new session that only uses one thread to do the actual data-sending.
	//This way we make sure all messages are received by us and send over the network in order.
	//if you dont care about order and need better performance, change it to a higher number.
	out.clear();
	out.event="core_NewSession";
	out["maxThreads"]=1;
	out.send();

}

int dataSessionId;
SYNAPSE_REGISTER(module_SessionStart)
{
	dataSessionId=msg.dst;
	///tell the rest of the world we are ready for duty
	Cmsg out;
	out.event="core_Ready";
	out.send();
}

// We extent the CnetMan class with our own network handlers.
// As soon as something with a network connection 'happens', these handlers will be called.
// In the case of this generic module, the data is assume to be readable text and is sended with a net_Read message.
class CnetModule : public CnetMan
{
	/** Connection 'id' is trying to connect to host:port
	* Sends: net_Connecting
	*/
 	void connecting(int id, string host, string port)
	{
		Cmsg out;
		out.dst=id;
		out.event="net_Connecting";
		out["host"]=host;
		out["port"]=port;
		out.send();
	}

	/** Connection 'id' is established.
	* Sends: net_Connected
	*/
 	void connected(int id)
	{
		Cmsg out;
		out.dst=id;
		out.src=dataSessionId;
		out.event="net_Connected";
		out.send();
	}

	/** Connection 'id' has received new data.
	* Sends: net_Read
	*/
	void read(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		Cmsg out;
		//TODO: isnt there a more efficient way to convert the streambuf to string?
		const char* s=boost::asio::buffer_cast<const char*>(readBuffer.data());
		//remove newline..
		out["data"].str().erase();
		out["data"].str().append(s,bytesTransferred-1);
		out.event="net_Read";
		out.dst=id;
		out.src=dataSessionId;
		out.send();
	}

	/** Connection 'id' is disconnected, or a connect-attempt has failed.
	* Sends: net_Disconnected
	*/
 	void disconnected(int id, const boost::system::error_code& ec)
	{
		Cmsg out;
		out.dst=id;
		out.event="net_Disconnected";
		out["reason"]=ec.message();
		out.send();
	}
};

CnetModule net;


/** Client-only: Create a new connection, connects to host:port for session src
 */
SYNAPSE_REGISTER(net_Connect)
{
	net.runConnect(msg.src, msg["host"], msg["port"]);
}

/** Server only: Creates a new server and listens specified port
 */
SYNAPSE_REGISTER(net_Listen)
{
	net.runListen(msg["port"]);
}

/** Server only: Accepts a connection on port and for session src
 */
SYNAPSE_REGISTER(net_Accept)
{
	net.runAccept(msg["port"], msg.src);

}

/** Server only: Stop listening on a port
 */
SYNAPSE_REGISTER(net_Close)
{
	net.doClose(msg["port"]);
}


/** Disconnections the connection related to src 
 */
SYNAPSE_REGISTER(net_Disconnect)
{
	net.doDisconnect(msg.src);
}

/** Write data to the connection related to src
 * If you care about data-ordering, send this to session-id that sended you the net_Connected.
 */
SYNAPSE_REGISTER(net_Write)
{
	//linebased, so add a newline
	msg["data"].str()+="\n";
	net.doWrite(msg.src, msg["data"]);
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



/** Network human interface device module.
 * This is to receive mappable keyboard input events via network, to control stuff.
 */

#include <string>

#include "synapse.h"
#include "cnetman.h"

int dataSessionId=0;
int moduleSessionId=0;
using namespace boost;
using namespace std;

/** module_Init - called first, set up basic stuff here
 */
SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	moduleSessionId=msg.dst;

	//max number of parallel threads
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=10;
	out.send();

	//The default session will be used to call the run-functions.
	//Every new connection needs a seperate thread.
	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=10;
	out.send();

	//Make a new session that only uses one thread to do the actual data-sending.
	//This way we make sure all messages are received by us and send over the network in order.
	//if you dont care about order and need better performance, change it to a higher number.
	out.clear();
	out.event="core_NewSession";
	out["maxThreads"]=1;
	out.send();

}


// We extent the Cnet class with our own network handlers.
// Every new network session will get its own Cnet object. 
// As soon as something with a network connection 'happens', these handlers will be called.
// In the case of this generic module, the data is assume to be readable text and is sended with a net_Read message.
class CnetModule : public synapse::Cnet
{

	
	//...put your per-network-connection variables here...
	


	/** Connection 'id' is established.
    * Used for both client and servers.
	* Sends: net_Connected
	*/
 	void connected(int id, const string &host, int port)
	{
		Cmsg out;
		out.dst=id;
		out.src=dataSessionId;
		out.event="nethid_Connected";
		out.send();
	}

	/** Connection 'id' has received new data.
	* Sends: net_Read
	*/
	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		Cmsg out;
		string dataStr(boost::asio::buffer_cast<const char*>(readBuffer.data()), readBuffer.size());
		dataStr.resize(dataStr.find(delimiter)+delimiter.length());

		//remove newline
		out["data"]=dataStr.substr(0, dataStr.length()-1);
		out.event="net_Read";
		out.dst=id;
		out.src=dataSessionId;
		out.send();

		readBuffer.consume(dataStr.length());
	}

	/** Connection 'id' is disconnected, or a connect-attempt has failed.
	* Sends: net_Disconnected
	*/
 	void disconnected(int id, const boost::system::error_code& ec)
	{
		Cmsg out;
		out.dst=id;
		out.event="nethid_Disconnected";
		out["reason"]=ec.message();
		out.send();
	}
}; 

synapse::CnetMan<CnetModule> net;

SYNAPSE_REGISTER(module_SessionStart)
{
	if (msg.dst=moduleSessionId)
		return;

	dataSessionId=msg.dst;
	///tell the rest of the world we are ready for duty
	Cmsg out;
	out.event="core_Ready";
	out.send();

	net.runListen(12345);

}


/** Server only: Accepts a connection on port and for session src
 */
SYNAPSE_REGISTER(net_Accept)
{
	net.runAccept(msg["port"], msg.src);
	Cmsg out;
	out.dst=msg.src;
	out.src=dataSessionId;
	out.event="net_AcceptEnded";
	out.send();

}



/** Write data to the connection related to src
 * If you care about data-ordering, send this to session-id that sended you the net_Connected.
 */
SYNAPSE_REGISTER(nethid_Write)
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



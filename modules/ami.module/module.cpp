#include "cnet.h"
#include "synapse.h"
#include <boost/regex.hpp>

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=10;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=10;
	out.send();

	///tell the rest of the world we are ready for duty
	out.clear();
	out.event="core_Ready";
	out.send();
}
 
class CnetModule : public Cnet
{
 	void connected(int id)
	{
		Cmsg out;
		out.dst=id;
		out.event="ami_Connected";
		out.send();
	}

	/** Parse an event from asterisk and convert it to a synapse message
	*/
	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		//convert streambuf to string
		string s(boost::asio::buffer_cast<const char*>(readBuffer.data()), bytesTransferred);

		DEB("Received:\n" << s);
		/* Example ami output:


		*/
		//parse lirc output
		smatch what;
		if (regex_match(
			s,
			what, 
			boost::regex("(.*?) (.*?) (.*?) (.*?)\n")
		))
		{
			//TODO: make a dynamicly configurable mapper, which maps lirc-events to other events.
			//(we'll do that probably after the gui-stuff is done)
			Cmsg out;
			out.event="lirc_Read";
			out["code"]		=what[1];
			out["repeat"]	=what[2];
			out["key"]		=what[3];
			out["remote"]	=what[4];
			out.send();
		}
		else
		{
			ERROR("Cant parse lirc output: " << s);
		}

	}

 	void disconnected(int id, const boost::system::error_code& ec)
	{
		Cmsg out;
		out.dst=id;
		out.event="ami_Disconnected";
		out["reason"]=ec.message();
		out.send();
	}


};

CnetMan<CnetModule> net;

SYNAPSE_REGISTER(ami_Connect)
{
	net.runConnect(msg.src, msg["host"], msg["port"], 5);
}

SYNAPSE_REGISTER(ami_Disconnect)
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
	INFO("ami shutting down...");
	//let the net module shut down to fix the rest
	net.doShutdown();
}

/** Send an action to asterisk
 */
SYNAPSE_REGISTER(ami_Action)
{
	string amiString;
	
	//convert synapse parameters to ami event string:
	for (Cmsg::iterator msgI=msg.begin(); msgI!=msg.end(); msgI++)
	{
		amiString+=(string)(msgI->first)+": "+(string)(msgI->second)+"\r\n";
	}
	amiString+="\r\n";
	
	DEB("Sending:\n" << amiString);

	net.doWrite(msg.src, amiString);
}

#include "cnetman.h"

#include "synapse.h"
#include <boost/regex.hpp>

/**
	Documentation: 

	To login to an asterisk server define these two handlers:
		SYNAPSE_REGISTER(ami_Ready)
		{
			Cmsg out;
			out.clear();
			out.event="ami_Connect";
			out["host"]="192.168.13.1";
			out["port"]="5038";
			out.send();
		}
		
		SYNAPSE_REGISTER(ami_Connected)
		{
			Cmsg out;
			out.clear();
			out.event="ami_Action";
			out["Action"]="Login";
			out["UserName"]="admin";
			out["Secret"]="yourpassword";
			out["ActionID"]="Login";
			out["Events"]="on";
			out.send();
		}

	Sending actions can be done with ami_Action. 

	Received events are converted to messages with event: ami_Event_"eventname" .

	Received responses are converted to messages with event: ami_Response_"responsename" .

	All parameters are copied from/to asterisk without modification.
*/

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
	void init(int id)
	{
		delimiter="\r\n\r\n";
	}
	
  	void connected(int id, const string &host, int port)
	{
		Cmsg out;
		out.dst=id;
		out.event="ami_Connected";
		out.send();
	}

	/** Parse an event from asterisk and convert it to a synapse message
	Events look like this ( \r\n as seperator ):
		Event: Newstate
		Privilege: call,all
		Channel: SIP/601-0000001d
		State: Up
		CallerID: 601
		CallerIDName: Erwin
		Uniqueid: 1267713042.34

	Responses look like this:
		Response: Success
		ActionID: Login
		Message: Authentication accepted


	*/
	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		//convert streambuf to string, and determine dataLength (can be less then bytesTransferred)
		string dataStr(boost::asio::buffer_cast<const char*>(readBuffer.data()), bytesTransferred);
		int dataLength=dataStr.find(delimiter)+delimiter.length();

		DEB("RECEIVED FROM ASTERISK:\n" << dataStr.substr(0,dataLength) );

		//create a regex iterator
		boost::sregex_iterator regexI(
			dataStr.begin(), 
			dataStr.begin()+dataLength, 
			boost::regex("^([[:alnum:]]*): (.*?)$")
		);

		Cmsg out;
		while (regexI!=sregex_iterator())
		{
			string key=(*regexI)[1].str();
			string value=(*regexI)[2].str();

			//assign it to the message hash-array:
			out[key]=value;

			if (key == "Event" )
				out.event="ami_Event_"+value;

			if (key == "Response" )
				out.event="ami_Response_"+value;

			regexI++;
		}
		out.send();

		readBuffer.consume(dataLength);
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
	
	DEB("SEND TO ASTERISK:\n" << amiString);

	net.doWrite(msg.src, amiString);
}

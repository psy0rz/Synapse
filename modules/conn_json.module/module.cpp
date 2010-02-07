#include "cnet.h"
#include "synapse.h"

#include "json_spirit.h"
#define MAX_MESSAGE 2048

//TODO: optimize: perhaps its more efficient to use boost spirit directly, without the json_spirit lib?

using namespace json_spirit;

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






// We extent the CnetMan class with our own network handlers.
// As soon as something with a network connection 'happens', these handlers will be called.
// This stuff basically runs as anonymous, until a user uses core_login to change the user.
class CnetModule : public CnetMan
{
	/** We started listening on a port
	*/
	void listening(int port)
	{
		//start a new anonymous acceptors to accept new incoming connections and handle the incomming data
		Cmsg out;
		out.event="core_NewSession";
		out["username"]="anonymous";
		out["password"]="anonymous";
		out["port"]=port;
		out.send();
	}

	/** Connection 'id' is trying to connect to host:port
	* Sends: net_Connecting
	*/
 	void connecting(int id, string host, string port)
	{
		Cmsg out;
		out.dst=id;
		out.event="conn_Connecting";
		out["host"]=host;
		out["port"]=port;
		out["type"]="json";
		out.send();
	}

	/** Connection 'id' is established.
	* Sends: conn_Connected
	*/
 	void connected(int id)
	{
		Cmsg out;
		out.dst=id;
		out.event="conn_Connected";
		out["type"]="json";
		out.send();
	}

	/** Recursively converts a json_spirit Value to a Cvar
	*/
	void Value2Cvar(Value &value,Cvar &var)
	{
		switch(value.type())
		{
			case(null_type):
				var.clear();
				break;
			case(str_type):
				var=value.get_str();
				break;
			case(real_type):
				var=value.get_real();
				break;
			case(obj_type):
				//convert the Object(string,Value) pairs to a CvarMap 
				for (Object::iterator ObjectI=value.get_obj().begin(); ObjectI!=value.get_obj().end(); ObjectI++)
				{
					//recurse to convert the map-value of the CvarMap into a json_spirit Value:
					Value2Cvar(ObjectI->value_, var[ObjectI->name_]);
				}
				break;
			default:
				WARNING("Ignoring unknown json variable type " << value.type());
				break;
		}
	}

	/** Connection 'id' has received new data.
	*/
	void read(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		Cmsg out;
	
		if (bytesTransferred > 1024)
		{
			WARNING("Json message on id " << id << " is " << bytesTransferred << " bytes. (max=" << MAX_MESSAGE << ")");
			return ;
		}

		//parse json input
		std::istream readBufferIstream(&readBuffer);
		try
		{
			Value jsonMsg;
			//TODO:how safe is it actually to let json_spirit parse untrusted input? (regarding DoS, buffer overflows, etc)
			json_spirit::read(readBufferIstream, jsonMsg);
			out.src=jsonMsg.get_array()[0].get_int();
			out.dst=jsonMsg.get_array()[1].get_int();
			out.event=jsonMsg.get_array()[2].get_str();
			Value2Cvar(jsonMsg.get_array()[3], out);
		}
		catch(...)
		{
			WARNING("Error while parsing incomming json message on id " << id);
			return ;
		}

		//translate....
		out.send();			
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

CnetModule net;




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


/** Recursively converts a Cvar to a json_spirit Value.
*/
void Cvar2Value(Cvar &var,Value &value)
{
	switch(var.which())
	{
		case(CVAR_EMPTY):
			value=Value();
			break;
		case(CVAR_STRING):
			value=(string)var;
			break;
		case(CVAR_LONG_DOUBLE):
			value=(double)var;
			break;
		case(CVAR_MAP):
			//convert the CvarMap to a json_spirit Object with (String,Value) pairs 
			value=Object();
			for (Cvar::iterator varI=var.begin(); varI!=var.end(); varI++)
			{
				Value subValue;
				//recurse to convert the map-value of the CvarMap into a json_spirit Value:
				Cvar2Value(varI->second, subValue);
				
				//push the resulting Value onto the json_spirit Object
				value.get_obj().push_back(Pair(
					varI->first,
					subValue
				));
			}
			break;
		default:
			WARNING("Ignoring unknown variable type " << var.which());
			break;
	}
}



/** This handler is called for all events that:
 * -dont have a specific handler, 
 * -are send to broadcast or to a session we manage.
 */
SYNAPSE_HANDLER(all)
{

	//convert the message to a json-message
	Array jsonMsg;
	jsonMsg.push_back(msg.src);	
	jsonMsg.push_back(msg.dst);
	jsonMsg.push_back(msg.event);
	
	//convert the parameters, if any
	if (!msg.isEmpty())
	{
		Value jsonPars;
		Cvar2Value(msg,jsonPars);
	
		jsonMsg.push_back(jsonPars);
	}

	INFO("json: " << write( jsonMsg));
}




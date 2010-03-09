#include "cnetman.h"
#include "synapse.h"
#include <boost/regex.hpp>

#include <fstream>
#include <ostream>

#include <sys/stat.h>

#include <cstdlib> 

#include "synapse_json.h"

#include "chttpsessionman.h"

#include <boost/lexical_cast.hpp>

#include "../../session.h"

#define MAX_CONTENT 20000


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


ChttpSessionMan httpSessionMan;


// We extent the Cnet class with our own network handlers.
// Every connection will get its own unique Cnet object.
// As soon as something with a network connection 'happens', these handlers will be called.
// We do syncronised writing instead of using the asyncronious doWrite()
class CnetModule : public Cnet
{
	//http basically has two states: The request-block and the content-body.
	//The states alter eachother continuesly during a connection.
	//We require different read, depending on the state we're in.
	//We have addition states for event polling
	enum Tstates {
		REQUEST,
		CONTENT,
		WAIT_LONGPOLL,
	};

	ThttpCookie httpCookie;
	int sessionId;
	Tstates state;
	string requestType;
	string requestUrl;
	Cvar headers;
//	Cvar cookies;


	void init_server(int id, CacceptorPtr acceptorPtr)
	{
		state=REQUEST;
		delimiter="\r\n\r\n";
		httpCookie=0;
		sessionId=SESSION_DISABLED;
	}

	void startAsyncRead()
	{
		if (state==REQUEST || state==WAIT_LONGPOLL)
		{
				DEB("Starting async read for REQUEST headers");
				//The request-block ends with a empty newline, so read until a double new-line:
				headers.clear();
				asio::async_read_until(
					tcpSocket,
					readBuffer,
					delimiter,
					bind(&Cnet::readHandler, this, _1, _2));
		}
		else 
		if (state==CONTENT)
		{
				//the buffer might already contain the data, so calculate how much more bytes we need:
				int bytesToTransfer=((int)headers["Content-Length"]-readBuffer.size());
				if (bytesToTransfer<0)
					bytesToTransfer=0;

				DEB("Starting async read for CONTENT, still need to receive " << bytesToTransfer << " of " << (int)headers["Content-Length"] << " bytes.");

				asio::async_read(
					tcpSocket,
					readBuffer,
					asio::transfer_at_least(bytesToTransfer),
					bind(&Cnet::readHandler, this, _1, _2));

		}
	}

	void sendHeaders(int status, Cvar & extraHeaders, bool partial=false)
	{
		stringstream statusStr;
		statusStr << status;

		string responseStr;
		responseStr+="HTTP/1.1 ";
		responseStr+=statusStr.str()+="\r\n";
		responseStr+="Server: synapse_http_json\r\n";
	
		//(re)send session cookie
		stringstream cookieStr;
		cookieStr << httpCookie;
		responseStr+="Set-Cookie: httpSession="+cookieStr.str()+"\r\n";

// 		for (Cvar::iterator varI=cookies.begin(); varI!=cookies.end(); varI++)
// 		{
// 			responseStr+="Set-Cookie: "+(string)(varI->first)+"="+(string)(varI->second)+"\r\n";
// 		}

		for (Cvar::iterator varI=extraHeaders.begin(); varI!=extraHeaders.end(); varI++)
		{
			responseStr+=(string)(varI->first)+": "+(string)(varI->second)+"\r\n";
		}
	
	
		if (partial)
		{
			DEB("Sending partial HEADERS: \n" << responseStr);
		}
		else
		{
			responseStr+="\r\n";
			DEB("Sending HEADERS: \n" << responseStr);
		}

		write(tcpSocket,asio::buffer(responseStr));
	}

	void respondString(int status, string data)
	{
		Cvar extraHeaders;
		extraHeaders["Content-Length"]=data.length();

		sendHeaders(status, extraHeaders);
		write(tcpSocket, asio::buffer(data));
	}

	void respondError(int status, string error)
	{
		WARNING("Responding with error: " << error);
		respondString(status, "<h1>Error</h1>"+error);
	}

	/** Respond by sending a file relative to the wwwdir/
	*/
	void respondFile(string path)
	{
		Cvar extraHeaders;

		//FIXME: do a better way of checking/securing the path. Inode verification?
		if (path.find("..")!=string::npos)
		{
			respondError(403, "Path contains illegal characters");
			return;
		}
		string localPath;
		localPath="wwwdir/"+path;
	
		struct stat statResults;
		int statError=stat(localPath.c_str(), &statResults);
		if (statError)
		{
			respondError(404, "Error while statting or file not found: " + path);
			return;
		}

		if (! (S_ISREG(statResults.st_mode)) )
		{
			respondError(404, "Path " + path + " is not a regular file");
			return;
		}	

		ifstream inputFile(localPath.c_str(), std::ios::in | std::ios::binary);
		if (!inputFile.good() )
		{
			respondError(404, "Error while opening " + path );
			return;
		}

		//determine filesize
		inputFile.seekg (0, ios::end);
		int fileSize=inputFile.tellg();
		inputFile.seekg (0, ios::beg);

		
		extraHeaders["Content-Length"]=fileSize;
		sendHeaders(200, extraHeaders);

		DEB("Sending CONTENT of " << path);
		char buf[1024];
		//TODO: is there a better way to do this?
		int sendSize=0;
		while (inputFile.good())
		{
			inputFile.read(buf	,sizeof(buf));
			write(tcpSocket, asio::buffer(buf, inputFile.gcount()));
			sendSize+=inputFile.gcount();
		}

		if (sendSize!=fileSize)
		{
			ERROR("Error during file transfer, disconnecting");
			doDisconnect();
		}
	}

	// Received new data:
	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		string dataStr(boost::asio::buffer_cast<const char*>(readBuffer.data()), readBuffer.size());
		string error;

		//parse http request headers
		if (state==REQUEST)
		{
			//resize data to first delimiter:
			dataStr.resize(dataStr.find(delimiter)+delimiter.length());
			readBuffer.consume(dataStr.length());

			DEB("Got http REQUEST: \n" << dataStr);

			//determine the kind of request:
 			smatch what;
 			if (!regex_match(
 				dataStr,
 				what, 
 				boost::regex("^(HEAD|GET|POST) (.*) HTTP/.*?$")
 			))
 			{
				error="Cant parse request.";
 			}
			else
			{
				requestType=what[1];
				requestUrl=what[2];

				//create a regex iterator for http headers
				boost::sregex_iterator headerI(
					dataStr.begin(), 
					dataStr.end(), 
					boost::regex("^([[:alnum:]-]*): (.*?)$")
				);
		
				//parse http headers
				while (headerI!=sregex_iterator())
				{
					string header=(*headerI)[1].str();
					string value=(*headerI)[2].str();
	
					headers[header]=value;	
					headerI++;
				}

				
				//we dont have a httpCookie for this connection yet, and browser sent us cookies?
				if (!httpCookie && headers.isSet("Cookie"))
				{
					//create a regex iterator for cookies
					boost::sregex_iterator cookieI(
						headers["Cookie"].str().begin(), 
						headers["Cookie"].str().end(), 
						boost::regex("([^=; ]*)=([^=; ]*)")
					);
			
					//parse cookies, to find httpSession cookie (if set)
					while (cookieI!=sregex_iterator())
					{
						if ((*cookieI)[1].str()=="httpSession")
						{
							try
							{
								httpCookie=lexical_cast<ThttpCookie>(((*cookieI)[2]).str());
							}
							catch(...)
							{
								WARNING("Invalid httpSession cookie: " << ((*cookieI)[2]).str()) ;
								httpCookie=0;
							}

							break;
						}
						cookieI++;
					}
				}

				//do we need a new http session cookie?
				if (!httpCookie)
				{
					httpCookie=httpSessionMan.newHttpSession();
				}
				

				//proceed based on requestType
				if (requestType=="POST")
				{
					if ( (int)headers["Content-Length"]<=0  || (int)headers["Content-Length"] > MAX_CONTENT )
					{
						error="Invalid Content-Length";
					}
					else
					{
						//ok, now change state to read the contents of the POST:
						state=CONTENT;
						return;
					}
				}
				else if (requestType=="GET")
				{
					//http://en.wikipedia.org/wiki/Comet_%28programming%29
					//Implements ajax long polling:
					if (requestUrl=="/synapse/longpoll")
					{
						//send partial headers now, and the content-length + json data later.
						Cvar extraHeaders;
						sendHeaders(200, extraHeaders,true);
						state=WAIT_LONGPOLL;
						DEB("Connection " << id << " is doing a long poll.");
					}
					else
					//Implements multipart/x-mixed-replace (only supported in firefox, but better performance)
					if (requestUrl=="/synapse/multipart")
					{
						//send headers directly, with content-type multipart/x-mixed-replace
						//TODO: implement me
					}
					else
					{
						//return requested page or events:
						respondFile(requestUrl);
					}
					return ;	
				}
			}
		}
		else 
		//we've received contents of a POST request.
		if (state==CONTENT)
		{
			//the next thing we should receive is another request, so change back state:
			state=REQUEST;			

			if (readBuffer.size() < headers["Content-Length"])
			{
				error="Didn't receive enough content-bytes!";
				DEB("ERROR: Expected " << (int)headers["Content-Length"] << " bytes, but only got: " << bytesTransferred);
			}
			else
			{
				
				dataStr.resize(headers["Content-Length"]);
				readBuffer.consume(headers["Content-Length"]);
				DEB("Got http CONTENT with length=" << dataStr.size() << ": \n" << dataStr);

				respondFile(requestUrl);

				return;
			}
		}
		else
		{
			error="Not expecting data in this state";
		}
		ERROR("Error while processing http data: " << error);
		error="Request aborted: "+error+"\n";
		write(tcpSocket,asio::buffer(error));
		doDisconnect();
		return;

	}


	/** Connection 'id' is disconnected, or a connect-attempt has failed.
	* Sends: conn_Disconnected
	*/
 	void disconnected(int id, const boost::system::error_code& ec)
	{
	}

	public:
	/** We receive this from synapse, with a message that COULD be for the connected client.
	* We might to write, queue or drop it.
    * We receive this call via the IOservice thread, so it runs in the same thread.
	*/
	void writeMessage(Cmsg & msg, string & jsonStr)
	{
		DEB("net " << id << " got " << jsonStr);
		if (state==WAIT_LONGPOLL)
		{

		}
	}

};

CnetMan<CnetModule> net;



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

	//other sessions are real sessions for clients, let httpSessionMan handle it
	httpSessionMan.sessionStart(msg);
}

SYNAPSE_REGISTER(module_NewSession_Error)
{
	httpSessionMan.newSessionError(msg);
}

/** This handler is called for all events that:
 * -dont have a specific handler, 
 * -are send to broadcast or to a session we manage.
 */
SYNAPSE_HANDLER(all)
{
	//encode to json, do it one time for performance reasons
	string jsonStr;
	Cmsg2json(msg, jsonStr);

	{
		//send the message to all open network connections.
		//CnetModule::sendMessage will handle the rest..
		lock_guard<mutex> lock(net.threadMutex);
		for (CnetMan<CnetModule>::CnetMap::iterator netI=net.nets.begin(); netI!=net.nets.end(); netI++)
		{
		//	netI->second->writeMessage(msg, jsonStr);
			netI->second->ioService.post(bind(&CnetModule::writeMessage,netI->second,msg,jsonStr));
		}
	}	
}




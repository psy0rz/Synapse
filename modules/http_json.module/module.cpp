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

#include "../../csession.h"

#include <queue>

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
	/// //////////////// PRIVATE STUFF FOR NETWORK CONNECTIONS

	private:

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
	Tstates state;
	string requestType;
	string requestUrl;
	Cvar headers;

	string jsonQueue;

	void init_server(int id, CacceptorPtr acceptorPtr)
	{
		state=REQUEST;
		delimiter="\r\n\r\n";
		httpCookie=0;
		cachedSessionId=SESSION_DISABLED;
		wantsMessages=false;
		jsonQueue.clear();
	}

	void startAsyncRead()
	{
		if (state==REQUEST || state==WAIT_LONGPOLL)
		{
				DEB(id << " starting async read for REQUEST headers");
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

				DEB(id << " starting async read for CONTENT, still need to receive " << bytesToTransfer << " of " << (int)headers["Content-Length"] << " bytes.");

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

	/** Writes the current json queue to the client.
	* Connection should be ready for it: e.g. browser did a longpoll, headers are sent, except for Content-Length.
	*/
	void respondJsonQueue()
	{
		if (!jsonQueue.empty())
		{
			//"close" the queue:
			jsonQueue+="]";
		}
		else
		{
			//respond with an empty array , instead of null:
			//(better for error handling in firefox)
			jsonQueue="[]";
		}

		//send headers
		Cvar extraHeaders;
		extraHeaders["Content-Length"]=jsonQueue.length();
		extraHeaders["Cache-Control"]="no-cache";
		extraHeaders["Content-Type"]="application/jsonrequest";
		sendHeaders(200, extraHeaders);

		//write the json queue
		write(tcpSocket, asio::buffer(jsonQueue));
		jsonQueue.clear();

		state=REQUEST;
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
		if (state==REQUEST || WAIT_LONGPOLL)
		{
			if (state==WAIT_LONGPOLL)
			{
				DEB("Cancelling longpoll on " << id);
				//the queue is still empty, so it responds with a content-length of 0 bytes:
				respondJsonQueue();
			}

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
								ThttpCookie clientsCookie=lexical_cast<ThttpCookie>(((*cookieI)[2]).str());
	
								//do we know the cookie the browser is giving us?
								//This is an expensive call, but its only done if httpCookie ist set yet
								if (httpSessionMan.isSessionValid(clientsCookie))
								{
									httpCookie=clientsCookie;
									break;
								}
								else
								{
									DEB(id << " ignored invalid or expired cookie " << clientsCookie);
								}
							}
							catch(...)
							{
								WARNING("Invalid httpSession format: " << ((*cookieI)[2]).str()) ;
								httpCookie=0;
							}
						}
						cookieI++;
					}
				}

				//do we need a new http session cookie for this connection?
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
						wantsMessages=true;


						if (!jsonQueue.empty())
						{
							//we have messages queued, so we can reply them directly:
							respondJsonQueue();
							DEB("WROTE QUEUED messages for httpSession " << httpCookie << " at connection " << id );
						}
						else
						{
							//we dont have messages, so wait for new messages.
							//this wait can be aborted if the browser sends another request.
							state=WAIT_LONGPOLL;
							DEB("Connection " << id << " is now waiting for long poll results.");
						}
					}
/*					else
					//TODO: implement me
					//Implements multipart/x-mixed-replace (only supported in firefox, but better performance)
					if (requestUrl=="/synapse/multipart")
					{
						//send headers directly, with content-type multipart/x-mixed-replace
						wantsMessages=true;
						state=??
					}*/
					else
					{
						//return requested page or events:
						respondFile(requestUrl);
						state=REQUEST;
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



	void enqueueJson(string & jsonStr)
	{
		if (jsonQueue.empty())
		{
			jsonQueue="["+jsonStr;
		}
		else
		{
			jsonQueue+=","+jsonStr;
		}
	}

	/// //////////////// PUBLIC INTERFACE FOR NETWORK CONNECTIONS
	public:
	bool wantsMessages;
	int cachedSessionId;

	/** We receive this from synapse, with a message that COULD be for the connected client.
	* We might to write, queue or drop it.
    * We receive this call via the IOservice thread, so it runs in the same thread.
	*/
	void writeMessage(int dstSessionId, string & jsonStr)
	{
		if (!wantsMessages)
		{
			WARNING("Dropping message for session " << dstSessionId << ": connection " << id << " doesnt want messages.");
			return;
		}

		//resolving a httpCookie to a session is an expensive call that involves locking, so cache it:
		if (cachedSessionId==SESSION_DISABLED)
		{
			cachedSessionId=httpSessionMan.getSessionId(httpCookie);
		}

		if (cachedSessionId==SESSION_DISABLED)
		{
			WARNING("Dropping message for session " << dstSessionId << ": httpSession " << httpCookie << " at connection " << id << " doesnt have a sessionId yet.");
			return;
		}

		if (cachedSessionId!=dstSessionId && dstSessionId!=0)
		{
			//this normally shouldnt happen, so its a warning:
			WARNING("Dropping message for session " << dstSessionId << ": httpSession " << httpCookie << " at connection " << id << " only wants " << cachedSessionId);
			return;
		}

		enqueueJson(jsonStr);

		//is the client waiting for json stuff?
		if (state==WAIT_LONGPOLL)
		{	
			//write it now
			respondJsonQueue();
			DEB("QUEUED and WROTE message for session " << dstSessionId << " for httpSession " << httpCookie << " at connection " << id );
		}
		else
		{
			DEB("QUEUED message for session " << dstSessionId << " for httpSession " << httpCookie << " at connection " << id );
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
			//NOTE: this is a lockless optimisation: we dont want to send the events to connections we can be 
			//SURE of that dont want or need it.
			if (netI->second->wantsMessages && 
				(msg.dst==0 || 
				 netI->second->cachedSessionId==msg.dst || 
				 netI->second->cachedSessionId==SESSION_DISABLED)
			)
			{
				netI->second->ioService.post(bind(&CnetModule::writeMessage,netI->second,msg.dst,jsonStr));
			}
		}
	}	
}




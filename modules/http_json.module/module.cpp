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

#include <boost/asio/buffer.hpp>

#include <algorithm>


#define MAX_CONTENT 20000


/**
	THIS MODULE IS UNFINISHED - DO NOT USE

	Sessions are identified with a cookie.
	The java script in the browser uses XMLhttprequest POST to post events. 
	The browser uses XMLhttprequest GET to receive events.
	We try to use multipart/x-mixed-replace to use peristent connections.

fout:
-broadcast multi moet aan, omdat we anders niet weten of een sessien wel permissies heeft om een event te ontvangen.

-queueing kan niet op connectie basis. 

-queueing op cookie of "instance" of "lastMessage-id" of sessie basis?

	

*/



int moduleSessionId=0;

int netIdCounter=0;

int moduleThreads=1;

map<string,string> contentTypes;


SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	moduleSessionId=msg.dst;

	//change module settings. 
	//especially broadcastMulti is important for our "all"-handler
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=moduleThreads;
	out["broadcastMulti"]=1;
	out.send();

	//we need multiple threads for network connection handling
	//(this is done with moduleThreads and sending core_ChangeModule events)
	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=10000;
	out.send();

	//register a special handler without specified event
	//this will receive all events that are not handled elsewhere in this module.
	out.clear();
	out.event="core_Register";
	out["handler"]="all";
	out.send();

	//set content types
	//TODO: load it from a file?
	contentTypes["css"]		="text/css";
	contentTypes["html"]	="text/html";
	contentTypes["js"]		="application/javascript";
	contentTypes["gif"]		="image/gif";
	contentTypes["jpeg"]	="image/jpeg";
	contentTypes["jpg"]		="image/jpeg";
	contentTypes["png"]		="image/png";


	//tell the rest of the world we are ready for duty
	out.clear();
	out.event="core_Ready";
	out.send();

}


ChttpSessionMan httpSessionMan;


// We extent the Cnet class with our own network handlers.
// Every connection will get its own unique Cnet object.
// As soon as something with a network connection 'happens', these handlers will be called.
// We do syncronised writing instead of using the asyncronious doWrite()
class CnetHttp : public Cnet
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

	ThttpCookie authCookie;
	Tstates state;
	string requestType;
	string requestUrl;
	string requestQuery;
	Cvar headers;


	void init_server(int id, CacceptorPtr acceptorPtr)
	{
		state=REQUEST;
		delimiter="\r\n\r\n";
		authCookie=0;
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
				int bytesToTransfer=((int)headers["content-length"]-readBuffer.size());
				if (bytesToTransfer<0)
					bytesToTransfer=0;

				DEB(id << " starting async read for CONTENT, still need to receive " << bytesToTransfer << " of " << (int)headers["content-length"] << " bytes.");

				asio::async_read(
					tcpSocket,
					readBuffer,
					asio::transfer_at_least(bytesToTransfer),
					bind(&Cnet::readHandler, this, _1, _2));

		}
	}

	/** Sends syncronious data to the tcpSocket and does error handling
    * NOTE: what happens if sendbuffer is full and client doesnt read data anymore? will Cnet hang forever?
	*/
	bool sendData(const asio::const_buffers_1 & buffers)
	{
		if (write(tcpSocket,buffers) != buffer_size(buffers))
		{
			ERROR(id << " had write while sending data, disconnecting.");
			doDisconnect();
			return(false);
		}
		return(true);
	}

	bool sendData(const asio::mutable_buffers_1 & buffers)
	{
		if (write(tcpSocket,buffers) != buffer_size(buffers))
		{
			ERROR(id << " had write while sending data, disconnecting.");
			doDisconnect();
			return(false);
		}
		return(true);
	}

	void sendHeaders(int status, Cvar & extraHeaders)
	{
		stringstream statusStr;
		statusStr << status;

		string responseStr;
		responseStr+="HTTP/1.1 ";
		responseStr+=statusStr.str()+="\r\n";
		responseStr+="Server: synapse_http_json\r\n";

// 		for (Cvar::iterator varI=cookies.begin(); varI!=cookies.end(); varI++)
// 		{
// 			responseStr+="Set-Cookie: "+(string)(varI->first)+"="+(string)(varI->second)+"\r\n";
// 		}

		for (Cvar::iterator varI=extraHeaders.begin(); varI!=extraHeaders.end(); varI++)
		{
			responseStr+=(string)(varI->first)+": "+(string)(varI->second)+"\r\n";
		}
		
		responseStr+="\r\n";
		DEB(id << " sending HEADERS: \n" << responseStr);
		
		sendData(asio::buffer(responseStr));

	}

	void respondString(int status, string data)
	{
		Cvar extraHeaders;
		extraHeaders["content-length"]=data.length();
		extraHeaders["content-type"]="text/html";

		sendHeaders(status, extraHeaders);
		sendData(asio::buffer(data));
	}

	void respondError(int status, string error)
	{
		WARNING(id << " responding with error: " << error);
		respondString(status, error);
	}



	/** Respond by sending a file relative to the wwwdir/
	*/
	bool respondFile(string path)
	{
		Cvar extraHeaders;

		//FIXME: do a better way of checking/securing the path. Inode verification?
		if (path.find("..")!=string::npos)
		{
			respondError(403, "Path contains illegal characters");
			return(true);
		}
		string localPath;
		localPath="wwwdir/"+path;
	
		struct stat statResults;
		int statError=stat(localPath.c_str(), &statResults);
		if (statError)
		{
			respondError(404, "Error while statting or file not found: " + path);
			return(true);
		}

		if (! (S_ISREG(statResults.st_mode)) )
		{
			respondError(404, "Path " + path + " is not a regular file");
			return(true);
		}	

		ifstream inputFile(localPath.c_str(), std::ios::in | std::ios::binary);
		if (!inputFile.good() )
		{
			respondError(404, "Error while opening " + path );
			return(true);
		}

		//determine content-type

		smatch what;
		if (regex_search(
			path,
			what, 
			boost::regex("([^.]*$)")
		))
		{
			string extention=what[1];
			if (contentTypes.find(extention)!=contentTypes.end())
			{
				extraHeaders["content-type"]=contentTypes[extention];
				DEB("Content type of ." << extention << " is " << contentTypes[extention]);
			}
			else
			{
				WARNING("Cannot determine content-type of ." << extention << " files");
			}
		}	

		//determine filesize
		inputFile.seekg (0, ios::end);
		int fileSize=inputFile.tellg();
		inputFile.seekg (0, ios::beg);

		
		extraHeaders["content-length"]=fileSize;
		sendHeaders(200, extraHeaders);

		DEB(id << " sending CONTENT of " << path);
		char buf[1024];
		//TODO: is there a better way to do this?
		int sendSize=0;
		while (inputFile.good())
		{
			inputFile.read(buf	,sizeof(buf));
			if (!sendData(asio::buffer(buf, inputFile.gcount())))
			{
				break;
			}
			sendSize+=inputFile.gcount();
		}

		if (sendSize!=fileSize)
		{
			ERROR(id <<  " error during file transfer, disconnecting");
			doDisconnect();
		}

		return true;
	}

	/** Responds with messages that are in the json queue for this clients session
	* If the queue is empty, it doesnt send anything and returns false. 
	* if it does respond with SOMETHING, even an error-response, it returns true.
    * Use abort to send an empty resonse without looking at the queue at all.
	*/
	bool respondJsonQueue(bool abort=false)
	{
		string jsonStr;
		if (abort)
		{
			//to abort we need to reply with an empty json array:
			DEB(id << " cancelling longpoll");
			jsonStr="[]";
			httpSessionMan.endGet(id, authCookie);
		}
		else
		{
			//check if there are messages in the queue, based on the authcookie from the current request:
			//This function will change authCookie if neccesary and fill jsonStr!
			httpSessionMan.getJsonQueue(id, authCookie, jsonStr);
	
			//authcookie was probably expired, respond with error
			if (!authCookie)
			{
				respondError(400, "Session is expired, or session limit reached.");
				return(true);
			}
		}

		if (jsonStr=="")
		{
			//nothing to reply (yet), retrun false and wait until there is a message
			DEB(id << " is now waiting for longpoll results");
			return(false);
		}
		else
		{
			//send headers
			Cvar extraHeaders;
			extraHeaders["content-length"]=jsonStr.length();
			extraHeaders["cache-control"]="no-cache";
			extraHeaders["content-type"]="application/json";
			extraHeaders["x-synapse-authcookie"]=authCookie;
			sendHeaders(200, extraHeaders);
	
			//write the json queue
			sendData(asio::buffer(jsonStr));
			return(true);
		}
	}


	/** This should be only called when the client is ready to receive a response to the requestUrl.
	* Returns true if something is sended.
	* Returns false if there is nothing to send yet. (longpoll mode)
    */
	bool respond(bool abort=false)
	{
		//someone requested the special longpoll url:
		if (requestUrl=="/synapse/longpoll")
		{
			return(respondJsonQueue(abort));
		}
		//just respond with a normal file
		else
		{
			return(respondFile(requestUrl));
		}
	}

	// Received new data:
	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		string dataStr(boost::asio::buffer_cast<const char*>(readBuffer.data()), readBuffer.size());
		string error;

		//we're expecting a new request:
		if (state==REQUEST || state==WAIT_LONGPOLL)
		{
			//if there is still an outstanding longpoll, cancel it:
			if (state==WAIT_LONGPOLL)
			{
				respond(true);
			}

			//resize data to first delimiter:
			dataStr.resize(dataStr.find(delimiter)+delimiter.length());
			readBuffer.consume(dataStr.length());

			DEB(id << " got http REQUEST: \n" << dataStr);

			//determine the kind of request:
 			smatch what;
 			if (!regex_search(
 				dataStr,
 				what, 
 				boost::regex("^(GET|POST) ([^? ]*)([^ ]*) HTTP/1..$")
 			))
 			{
				error="Cant parse request.";
 			}
			else
			{
				requestType=what[1];
				requestUrl=what[2];
				requestQuery=what[3];
				DEB("REQUEST query: " << requestQuery);

				//create a regex iterator for http headers
				headers.clear();
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
	
					//headers handling is lowercase in synapse!
					transform(header.begin(), header.end(), header.begin(), ::tolower);
	
					headers[header]=value;	
					headerI++;
				}

				//this header MUST be set on longpoll requests:
				//if the client doesnt has one yet, httpSessionMan will assign a new one.
				authCookie=headers["x-synapse-authcookie"];

				//proceed based on requestType:
				//a GET or empty POST:
				if (requestType=="GET" || (int)headers["content-length"]==0)
				{
					if (respond())
						state=REQUEST;
					else
						state=WAIT_LONGPOLL;
					return;	
				}
				//a POST with content:
				else 
				{
					if ( (int)headers["content-length"]<0  || (int)headers["content-length"] > MAX_CONTENT )
					{
						error="Invalid Content-Length";
					}
					else
					{
						//change state to read the contents of the POST:
						state=CONTENT;
						return;
					}
				}
			}
		}
		else 
		//we're expecting the contents of a POST request.
		if (state==CONTENT)
		{
			if (readBuffer.size() < headers["content-length"])
			{
				error="Didn't receive enough content-bytes!";
				DEB(id <<  " ERROR: Expected " << (int)headers["content-length"] << " bytes, but only got: " << bytesTransferred);
			}
			else
			{				
				dataStr.resize(headers["content-length"]);
				readBuffer.consume(headers["content-length"]);
				DEB(id << " got http CONTENT with length=" << dataStr.size() << ": \n" << dataStr);

				//POST to the special send url?
				if (requestUrl=="/synapse/send")
				{
					error=httpSessionMan.sendMessage(authCookie, dataStr);
					if (error=="")
						respondString(200, "");
					else
						respondError(400, error);

					state=REQUEST;
					return;
				}
				else
				{
					//ignore whatever is posted, and just respond normally:
					if (respond())
						state=REQUEST;
					else
						state=WAIT_LONGPOLL;
					return;
				}
			}
		}

		//ERROR(id << " had error while processing http data: " << error);
		error="Bad request: "+error;
		respondError(400, error);
		doDisconnect();
		return;

	}


	/** Connection 'id' is disconnected, or a connect-attempt has failed.
	* Sends: conn_Disconnected
	*/
 	void disconnected(int id, const boost::system::error_code& ec)
	{
		httpSessionMan.endGet(id, authCookie);
	}




	/// //////////////// PUBLIC INTERFACE FOR NETWORK CONNECTIONS
	public:

	/** We receive this when the queue is changed and we are (probably) waiting for a message.
	 * I say PROBABLY, because the client could have changed its mind in the meanwhile.
	 */
	void queueChanged()
	{
		//are we really waiting for something?
		if (state==WAIT_LONGPOLL)
		{
			//try to respond (this will call the httpSessionMan to see if there really is a message for our authCookie)
			if (respondJsonQueue())
			{
				//we responded, change our state back to REQUEST
				state=REQUEST;
			}
		}
	}

	/** We receive this from synapse, with a message that COULD be for the connected client.
	* We might to write, queue or drop it.
    * We receive this call via the IOservice thread, so it runs in the same thread.
	*/
// 	void writeMessage(int dstSessionId, string & jsonStr)
// 	{
// 		if (!wantsMessages)
// 		{
// 			WARNING(id << " dropping message for session " << dstSessionId << ": connection " << id << " doesnt want messages.");
// 			return;
// 		}
// 
// 		//resolving a httpCookie to a session is an expensive call that involves locking, so cache it:
// 		if (cachedSessionId==SESSION_DISABLED)
// 		{
// 			cachedSessionId=httpSessionMan.getSessionId(httpCookie);
// 		}
// 
// 		if (cachedSessionId==SESSION_DISABLED)
// 		{
// 			WARNING(id << " dropping message for session " << dstSessionId << ": httpSession " << httpCookie << " at connection " << id << " doesnt have a sessionId yet.");
// 			return;
// 		}
// 
// 		if (cachedSessionId!=dstSessionId && dstSessionId!=0)
// 		{
// 			//this normally shouldnt happen, so its a warning:
// 			WARNING(id << " dropping message for session " << dstSessionId << ": httpSession " << httpCookie << " at connection " << id << " only wants " << cachedSessionId);
// 			return;
// 		}
// 
// 		enqueueJson(jsonStr);
// 
// 		//is the client waiting for json stuff?
// 		if (state==WAIT_LONGPOLL)
// 		{	
// 			//respond now!
// 			respond();
// 			DEB(id << " QUEUED and WROTE message for session " << dstSessionId << " for httpSession " << httpCookie );
// 		}
// 		else
// 		{
// 			DEB(id << " QUEUED message for session " << dstSessionId << " for httpSession " << httpCookie);
// 		}		
// 	}

};

CnetMan<CnetHttp> net;



/** Creates a new server and listens specified port
 */
SYNAPSE_REGISTER(http_json_Listen)
{
	if (msg.dst==moduleSessionId)
	{
		//starts a new thread to accept and handle the incomming connection.
		Cmsg out;
		int connections;	

		if (msg.isSet("connections"))
		{
			connections=msg["connections"];
		}
		else
		{
			//default is 100 connections
			connections=100;		
		}


		//allow this many threads.
		//NOTE: the reason why we do this, is to prevent multiple threads from entereing our "all" handler: this would be bad for performance, since httpSessionMan is locked to 1 thread.
		moduleThreads+=connections+1;
		out.clear();
		out.event="core_ChangeModule";
		out["maxThreads"]=moduleThreads;
		out.send();
		
		//fire off acceptor threads
		out.clear();
		out.dst=moduleSessionId;
		out.event="http_json_Accept";
		out["port"]=msg["port"];
		for (int i=0; i<connections; i++)
	 		out.send();

		//become the listening thread
		net.runListen(msg["port"]);

		//restore max module threads
		moduleThreads-=(connections+1);
		out.clear();
		out.event="core_ChangeModule";
		out["maxThreads"]=moduleThreads;
		out.send();
	}
	else
		ERROR("Send to the wrong session id");
	
}


/** Runs an acceptor thread (only used internally)
 */
SYNAPSE_REGISTER(http_json_Accept)
{
		//race condition with net.runlisten..
		sleep(1);
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

/** Enqueues the message for the appropriate httpSession, and informs any waiting Cnet network connections.
*/
void enqueueMessage(Cmsg & msg, int dst)
{
	//ignore these sessions
	if (dst==moduleSessionId)
		return;

	//pass the message to the http session manager, he knows what to do with it!
	int waitingNetId=httpSessionMan.enqueueMessage(msg,dst);
	if (waitingNetId)
	{
		//a client is waiting for a message, lets inform him:
		lock_guard<mutex> lock(net.threadMutex);

		CnetMan<CnetHttp>::CnetMap::iterator netI=net.nets.find(waitingNetId);

		//client connection still exists?
		if (netI != net.nets.end())
		{
			DEB("Informing net " << waitingNetId << " of queue change.");
			netI->second->ioService.post(bind(&CnetHttp::queueChanged,netI->second));
		}
		else
		{
			DEB("Want to inform net " << waitingNetId << " of queue change, but it doesnt exist anymore?");
		}
	}
}


SYNAPSE_REGISTER(module_SessionStart)
{
	if (dst==moduleSessionId)
		return;

	httpSessionMan.sessionStart(msg);
	enqueueMessage(msg,dst);
}

SYNAPSE_REGISTER(module_NewSession_Error)
{
	if (dst==moduleSessionId)
		return;

	httpSessionMan.newSessionError(msg);
	//we cant: enqueueMessage(msg,dst);
	
}

SYNAPSE_REGISTER(module_SessionEnd)
{
	if (dst==moduleSessionId)
		return;

	httpSessionMan.sessionEnd(msg);
	//we cant: enqueueMessage(msg,dst);
}

/** This handler is called for all events that:
 * -dont have a specific handler, 
 * -are send to broadcast or to a session we manage.
 */
SYNAPSE_HANDLER(all)
{
	enqueueMessage(msg, dst);
}




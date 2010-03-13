#include <time.h>
#include "chttpsessionman.h"
#include <sstream>
#include "../../csession.h"

#include "synapse_json.h"

using namespace std;


ChttpSessionMan::ChttpSessionMan()
{
	//FIXME: unsafe randomiser?
	srand48_r(time(NULL), &randomBuffer);
}

//A client wants to pop the queued messages for the specified authCookie.
//If the cookie is invalid, we overwrite it with a new one and request a new session from the core.
//If the queue is still empty, jsonStr will be empty as well.
//If there are messages in the queue, they are moved to jsonStr and the queue will be empty again.
void ChttpSessionMan::getJsonQueue(int netId, ThttpCookie & authCookie, string & jsonStr)
{
	lock_guard<mutex> lock(threadMutex);

	//find the session by searching for the authCookie
	//TODO: optimize by letting the caller supply a sessionIdHint.
	ChttpSessionMap::iterator httpSessionI=httpSessionMap.begin();
	while (httpSessionI!=httpSessionMap.end())
	{
		//found it!
		if (httpSessionI->second.authCookie==authCookie)
		{
			if (!httpSessionI->second.jsonQueue.empty())
			{
				//return the queued json messages: 
				jsonStr=httpSessionI->second.jsonQueue+"]";
				//clear the queue and netId
				httpSessionI->second.jsonQueue.clear();
				httpSessionI->second.netId=0;
			}
			else
			{
				//we dont have messages yet, the client will wait for us, so remember the netId of that client so we can inform it as soon as something arrives.
				jsonStr.clear();
				httpSessionI->second.netId=netId;
			}
			return ;
		}			
		httpSessionI++;
	}

	DEB("Unknown or expired authCookie: " << authCookie << ", requesting new session.");
	
	//we cannot find the authCookie.
	//probably because the client doesnt has one, or because its expired.
	//lets create a new authCookie:
	mrand48_r(&randomBuffer, &authCookie);

	//Request a new session from core.
	//we add some extra information that helps us find back the client
	Cmsg out;
	out.event="core_NewSession";
	out["username"]="anonymous";
	out["password"]="anonymous";
	out["authCookie"]=authCookie;
	out["netId"]=netId;
	out.send();


	//TODO: cleanup old or unused sessions


}

//core informs us of a new session that is started, probably for a client of us:
void ChttpSessionMan::sessionStart(Cmsg & msg)
{
	lock_guard<mutex> lock(threadMutex);
 
	if (!msg.isSet("authCookie"))
	{
		WARNING("Ignoring new session " << msg.dst << " from core: no authCookie set?");
		return;
	}

	ChttpSessionMap::iterator httpSessionI=httpSessionMap.find(msg.dst);
 	if (httpSessionI!=httpSessionMap.end())
	{
		WARNING("Session " << msg.dst << ", already exists in httpSessionMan?");
		return;
	}

	httpSessionMap[msg.dst].authCookie=msg["authCookie"];
	httpSessionMap[msg.dst].netId=msg["netId"];

	DEB("Created new httpSession " << msg.dst << ", with authCookie " << msg["authCookie"].str());

	//(client will get informed because sendMessage will be called as well)
}

//session creation failed
void ChttpSessionMan::newSessionError(Cmsg & msg)
{
	lock_guard<mutex> lock(threadMutex);
	//TODO: inform the client somehow the session creation failed?
}

//core informs us a session has ended.
void ChttpSessionMan::sessionEnd(Cmsg & msg)
{
	lock_guard<mutex> lock(threadMutex);
	//TODO: inform the client somehow the session ended?
	httpSessionMap.erase(msg.dst);
}

//core has a message for us, add it to the message-queue of the corresponding session:
//returns a netId which is >0, if a network connection needs to be hinted of the change.
int ChttpSessionMan::sendMessage(Cmsg & msg)
{
	lock_guard<mutex> lock(threadMutex);
	ChttpSessionMap::iterator httpSessionI=httpSessionMap.find(msg.dst);
	if (httpSessionI==httpSessionMap.end())
	{
		WARNING("Dropped message for session " << msg.dst << ": session not found");
		return(0);
	}

	//convert message to json
	string jsonStr;
	Cmsg2json(msg, jsonStr);

	//add it to the jsonQueue. Which is a string that contains a json-array.
	if (httpSessionI->second.jsonQueue.empty())
	{
		httpSessionI->second.jsonQueue="["+jsonStr;
	}
	else
	{
		httpSessionI->second.jsonQueue+=","+jsonStr;
	}

	//TODO: check if the queue gets too big and session needs to be killed.

	//we want to inform the netId only ONE time
	int netId=httpSessionI->second.netId;
	httpSessionI->second.netId=0;

	DEB("Enqueued message, probably for netId " << netId << ": " << jsonStr);

	return(netId);
}


 





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

	//important limits:
	maxSessionIdle=10;
	maxSessions=100;
	maxSessionQueue=100000;
}



ChttpSessionMan::ChttpSessionMap::iterator ChttpSessionMan::findSessionByCookie(ThttpCookie & authCookie)
{
	//find the session by searching for the authCookie
	//TODO: optimize by letting the caller supply a sessionIdHint.
	ChttpSessionMap::iterator httpSessionI=httpSessionMap.begin();
	while (httpSessionI!=httpSessionMap.end())
	{
		//found it!
		if (httpSessionI->second.authCookie==authCookie)
		{
			//remember the session is still used
			httpSessionI->second.lastTime=time(NULL);
			return (httpSessionI);
		}
		httpSessionI++;
	}
	return(httpSessionI);
}




/** A client wants to pop the queued messages for the specified authCookie.
//If the authCookie is invalid, we overwrite authCookie with 0 and we abort.
//
//if the authCookie is 0, we create a new session and store it in authCookie.
//
//If the queue is empty, jsonStr will be empty as well. The netId will be remembered so that enqueueMessage() can return it to cmodule.cpp, which in turn will inform the network-session of a queue change.
//
//If there are messages in the queue, they are moved to jsonStr and the queue will be empty again.
*/
void ChttpSessionMan::getJsonQueue(int netId, ThttpCookie & authCookie, string & jsonStr)
{
	lock_guard<mutex> lock(threadMutex);

	if (authCookie!=0)
	{
		ChttpSessionMap::iterator httpSessionI=findSessionByCookie(authCookie);
	
		if (httpSessionI==httpSessionMap.end())
		{
			DEB("Unknown or expired authCookie: " << authCookie << ", aborting.");
			authCookie=0;
			return;
		}

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
		return;
	}

	//expire old sessions, before creating new ones :)
	expireCheckAll();

	if (httpSessionMap.size() >= maxSessions)
	{
		WARNING("Too many httpSessions, not creating new session for netId " << netId);
		return;
	}

	DEB("Creating new session for " << netId);
	
	//create a new uniq authCookie:
	while (!authCookie || findSessionByCookie(authCookie)!=httpSessionMap.end()) 
	{
		mrand48_r(&randomBuffer, &authCookie);
	} 

	//Request a new session from core.
	//we add some extra information that helps us find back the client
	Cmsg out;
	out.event="core_NewSession";
	out["username"]="anonymous";
	out["password"]="anonymous";
	out["authCookie"]=authCookie;
	out["netId"]=netId;
	out.send();
}

/** Called from to indicate that the network session is not interested anymore.
 * Its neccesary to get informed of this, so we know when to expire session.
 */
void ChttpSessionMan::endGet(int netId,ThttpCookie & authCookie)
{
	//if theres netId or authCookie, we can just ignore it without locking
	if (!netId || !authCookie)
		return;

	{
		lock_guard<mutex> lock(threadMutex);
	
		ChttpSessionMap::iterator httpSessionI=findSessionByCookie(authCookie);
	
		if (httpSessionI==httpSessionMap.end())
		{
			DEB("Network id " << netId << " with UNKNOWN httpsession " << authCookie << " isnt interested in messages anymore. ignoring."); 
			return;
		}
	
		if (httpSessionI->second.netId!=netId)
		{
			DEB("UNKNOWN network id " << netId << " with httpsession " << authCookie << " isnt interested in messages anymore. ignoring."); 
			return;
		}
	
		DEB("Network id " << netId << " with httpsession " << authCookie << " isnt interested in messages anymore. Session timeout timing is now enable."); 
		httpSessionI->second.netId=0;
	}	
}


/** A client wants to send a message to the core 
 */
string ChttpSessionMan::sendMessage(ThttpCookie & authCookie, string & jsonStr)
{
	stringstream error;
	Cmsg msg;

	//we do this unlocked, since parsing probably takes most of the time:
	if (!json2Cmsg(jsonStr, msg))
	{	
		error <<  "Error while parsing JSON message:" << jsonStr;
		return (error.str());
	}

	{
		//httpSession stuff, has to be locked offcourse:
		lock_guard<mutex> lock(threadMutex);
		ChttpSessionMap::iterator httpSessionI=findSessionByCookie(authCookie);
	
		if (httpSessionI==httpSessionMap.end())
		{
			error << "Cannot send message, authCookie " << authCookie << " not found.";
			return (error.str());
		}

		//fill in msg.src with the correct session id
		msg.src=httpSessionI->first;
	}

	if (!msg.send())
	{
		error << "Error while sending message. (no permission?)";
		return (error.str());
	}

	return ("");
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
//	lock_guard<mutex> lock(threadMutex);
	//we cant do anything at this point...the client will just wait forever.
	//if our maxSessions has a practical value this should never happen.
}

//core informs us a session has ended.
void ChttpSessionMan::sessionEnd(Cmsg & msg)
{
	lock_guard<mutex> lock(threadMutex);
	DEB("Core has ended session " << msg.dst << ", deleting httpSession.");
	httpSessionMap.erase(msg.dst);

}

//core has a message for us, add it to the message-queue of the corresponding session:
//returns a netId which is >0, if a network connection needs to be hinted of the change.
int ChttpSessionMan::enqueueMessage(Cmsg & msg, int dst)
{
	//convert message to json
	//we do this without lock ,since it can be a "slow" operation:
	string jsonStr;
	Cmsg2json(msg, jsonStr);

	{
		lock_guard<mutex> lock(threadMutex);
		//NOTE: look at dst and NOT at msg.dst, because of broadcasts!
	
		ChttpSessionMap::iterator httpSessionI=httpSessionMap.find(dst);
		if (httpSessionI==httpSessionMap.end())
		{
			WARNING("Dropped message for session " << dst << ": session not found");
			return(0);
		}

		//check if the session is expired:
		expireCheck(httpSessionI);
		if (httpSessionI->second.expired)
		{
			DEB("Dropped message for session " << dst << ": session expired");
			return(0);
		}
	
		//add it to the jsonQueue. Which is a string that contains a json-array.
		if (httpSessionI->second.jsonQueue.empty())
		{
			httpSessionI->second.jsonQueue="["+jsonStr;
		}
		else
		{
			httpSessionI->second.jsonQueue+=","+jsonStr;
		}
	
		//we want to inform the netId only ONE time
		int netId=httpSessionI->second.netId;
		httpSessionI->second.netId=0;
	
		DEB("Enqueued message for destination session " << dst << ", probably for netId " << netId << ": " << jsonStr);
	
		return(netId);
	}
}

void ChttpSessionMan::expireCheck(ChttpSessionMap::iterator httpSessionI)
{
	//already expired?, no need to check again
	if (httpSessionI->second.expired)
	{
		return;
	}

	//an active network session is currenty waiting? never expire it if this is the case.
	if (httpSessionI->second.netId)
	{
		return;
	}

	//session timeout?
	if ((time(NULL)-(httpSessionI->second.lastTime)) > maxSessionIdle)
	{
		DEB("Ending old session: " << httpSessionI->first << " with authCookie " << httpSessionI->second.authCookie << ".");

		httpSessionI->second.expired=true;
	}

	//queue too big?
	if (httpSessionI->second.jsonQueue.length() > maxSessionQueue)
	{
		WARNING("While trying to send to session " << httpSessionI->first << " with authCookie " << httpSessionI->second.authCookie << " queue overflowed, ending session.");
		httpSessionI->second.expired=true;
	}

	if (httpSessionI->second.expired)
	{
		//ask core to delete session, after which our sessionEnd() will erase it from the session map:
		Cmsg out;
		out.src=httpSessionI->first;
		out.event="core_DelSession";
		out.send();
	}

}

void ChttpSessionMan::expireCheckAll()
{
	ChttpSessionMap::iterator httpSessionI=httpSessionMap.begin();
	while (httpSessionI!=httpSessionMap.end())
	{
		expireCheck(httpSessionI);
		httpSessionI++;
	}
}





/*  Copyright 2008,2009,2010 Edwin Eefting (edwin@datux.nl) 

    This file is part of Synapse.

    Synapse is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Synapse is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Synapse.  If not, see <http://www.gnu.org/licenses/>. */


#include <time.h>
#include "chttpsessionman.h"
#include <sstream>
#include "../../csession.h"


using namespace std;


ChttpSessionMan::ChttpSessionMan()
{
	//FIXME: unsafe randomiser?
	srand48_r(time(NULL), &randomBuffer);

	//important limits:
	maxSessionIdle=20;
	maxSessions=200;
	maxSessionQueue=500000;
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
void ChttpSessionMan::getJsonQueue(int netId, ThttpCookie & authCookie, string & jsonStr, ThttpCookie authCookieClone)
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
			httpSessionI->second.netInformed=false;
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

	//Request a new session from core...
	Cmsg out;
	out.event="core_NewSession";
	out["description"]="http_json session";

	//Should we try to clone the credentials from a previous authCookie?
	ChttpSessionMap::iterator httpSessionI=findSessionByCookie(	authCookieClone);
	if (httpSessionI!=httpSessionMap.end())
	{
		//request the new session from the existing session, this way it gets the same credentials:
		out.src=httpSessionI->first;
		out["authCookieClone"]=authCookieClone;
		DEB("Cloning existing credentials from previous session " << authCookieClone << " to new session " << authCookie);
	}
	else
	{
		//no existing session (anymore), so just create an anonymous session
		out["username"]="anonymous";
		out["password"]="anonymous";
	}

	//we add some extra information that helps us find back the client
	out["authCookie"]=authCookie;
	out["netId"]=netId;

	try
	{
		out.send();
	}
	catch(...)
	{
		//sending fails if the user doesnt have rights to send a NewSession request. (like anonymous user)
		//in this case we just create a new anonymous session without cloning.
		out.src=0;
		out["username"]="anonymous";
		out["password"]="anonymous";
		out.send();
	}
}

/** Simpeller version, that is used to only get the queue if its there, without affecting netId waiting or creating sessions.
 *  This is used to efficiently return the json queue while the client is sending a message.
 *
 */
void ChttpSessionMan::getJsonQueue(ThttpCookie & authCookie, string & jsonStr)
{
	lock_guard<mutex> lock(threadMutex);

	if (authCookie!=0)
	{
		ChttpSessionMap::iterator httpSessionI=findSessionByCookie(authCookie);

		if (httpSessionI!=httpSessionMap.end())
		{
			DEB("Unknown or expired authCookie: " << authCookie << ", aborting.");
			if (!httpSessionI->second.jsonQueue.empty())
			{
				//return the queued json messages:
				jsonStr=httpSessionI->second.jsonQueue+"]";
				//clear the queue
				httpSessionI->second.jsonQueue.clear();
			}
			else
			{
				jsonStr="[]";
			}
		}
	}
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
string ChttpSessionMan::sendMessage(ThttpCookie & authCookie, string & jsonLines)
{
	stringstream error;
	Cmsg msg;

	//multiple json messages can be send seperated by newlines.
	unsigned int nextPos=0;
	unsigned int pos=0;
	while(pos<jsonLines.length())
	{
		bool failed=false;

		//find position of next newline
		nextPos=jsonLines.find('\n',pos+1);
		if (nextPos==string::npos)
			nextPos=jsonLines.length();


		//we do this unlocked, since parsing probably takes most of the time:
		try
		{
			string jsonStr=jsonLines.substr(pos,nextPos-pos);
			msg.fromJson(jsonStr);
		}
		catch(std::exception &e)
		{
			error <<  "Error while parsing JSON message:" << e.what() << "\n ";
			failed=true;
		}

		if (!failed)
		{
			//httpSession stuff, has to be locked offcourse:
			lock_guard<mutex> lock(threadMutex);
			ChttpSessionMap::iterator httpSessionI=findSessionByCookie(authCookie);

			if (httpSessionI==httpSessionMap.end())
			{
				error << "Cannot send message, authCookie " << authCookie << " not found.\n ";
				failed=true;
			}

			//fill in msg.src with the correct session id
			msg.src=httpSessionI->first;
		}
	
		if (!failed)
		{
			try
			{
				msg.send();
			}
			catch(const std::exception& e)
			{
				error << string(e.what()) << "\n ";
				failed=true;
			}
		}

		pos=nextPos;
	}

	return(error.str());
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
	msg.toJson(jsonStr);

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
		int netId=0;
		if (!httpSessionI->second.netInformed)
		{
			httpSessionI->second.netInformed=true;
			netId=httpSessionI->second.netId;
		}
	
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

string ChttpSessionMan::getStatusStr()
{
	lock_guard<mutex> lock(threadMutex);

	stringstream s;

	ChttpSessionMap::iterator httpSessionI=httpSessionMap.begin();
	while (httpSessionI!=httpSessionMap.end())
	{
		s << "synapseId " << httpSessionI->first << ": ";
		s << httpSessionI->second.getStatusStr();
		s << "\n";
		httpSessionI++;
	}
	return (s.str());
}


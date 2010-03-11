#include <time.h>
#include "chttpsessionman.h"
#include <sstream>
#include "../../csession.h"

using namespace std;


ChttpSessionMan::ChttpSessionMan()
{
	//FIXME: unsafe randomiser?
	srand48_r(time(NULL), &randomBuffer);
}


// int ChttpSessionMan::getNetworkId(int sessionId)
// {
// }
// 
// 
// int ChttpSessionMan::getNetworkId(int sessionId)
// {
// }

ThttpCookie ChttpSessionMan::newHttpSession()
{
	lock_guard<mutex> lock(threadMutex);

	ThttpCookie httpCookie;
	mrand48_r(&randomBuffer, &httpCookie);

	//add to session map
	//TODO: cleanup old or unused sessions
	httpSessionMap[httpCookie];

	//request new session from core
	Cmsg out;
	out.event="core_NewSession";
	out["username"]="anonymous";
	out["password"]="anonymous";
	out["httpSession"]=httpCookie;
	out.send();

	return (httpCookie);
}

bool ChttpSessionMan::isSessionValid(ThttpCookie httpCookie)
{
	lock_guard<mutex> lock(threadMutex);
	return (httpSessionMap.find(httpCookie)!=httpSessionMap.end());
}

int ChttpSessionMan::getSessionId(ThttpCookie httpCookie)
{
	lock_guard<mutex> lock(threadMutex);

	if (!httpCookie)
		return SESSION_DISABLED;

	ChttpSessionMap::iterator httpSessionI=httpSessionMap.find(httpCookie);
 	if (httpSessionI!=httpSessionMap.end())
		return httpSessionI->second.sessionId;
	else
		return SESSION_DISABLED;
 
}


void ChttpSessionMan::sessionStart(Cmsg & msg)
{
	lock_guard<mutex> lock(threadMutex);
	ChttpSessionMap::iterator httpSessionI=httpSessionMap.find(msg["httpSession"]);
 
 	if (httpSessionI==httpSessionMap.end())
	{
		WARNING("Got new session " << msg.dst << ", but cant find httpSession " << msg["httpSession"].str());
	}
	else
	{
		if (httpSessionI->second.sessionId!=SESSION_DISABLED)
		{
			WARNING("Cannot link session " << msg.dst << " to httpSession " << msg["httpSession"].str() << ": its already linked to " << httpSessionI->second.sessionId);
		}
		else
		{
			httpSessionI->second.sessionId=msg.dst;
			DEB("Linked httpSession " << msg["httpSession"].str() << " to session " << msg.dst);
			return;
		}
	}

	//if we cant link the session, deleted it
	Cmsg out;
	out.event="core_DelSession";
	out.src=msg.dst;
	out.send();
}

void ChttpSessionMan::newSessionError(Cmsg & msg)
{

}

void ChttpSessionMan::sendMessage(Cmsg & msg)
{

}




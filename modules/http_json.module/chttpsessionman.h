/** 

public stuff of httpSessionMan is thread safe.
httpSession is NOT thread safe and should never be access directly from the outside.

httpSession is only used by httpSessionMan.
httpSessionMan is only used by stuff in module.cpp
httpSessionMan and httpSession NEVER call module.cpp stuff themself, to keep things clean and clear.


*/
#ifndef CHTTPSESSIONMAN_H
#define CHTTPSESSIONMAN_H

#include <map>
#include <stdlib.h>
#include <string>
#include "chttpsession.h"
#include <boost/thread/mutex.hpp>
#include "cmsg.h"


using namespace boost;
using namespace std;


class ChttpSessionMan
{
	typedef map<int, ChttpSession> ChttpSessionMap;
	ChttpSessionMap httpSessionMap;

	mutex threadMutex;
 	struct drand48_data randomBuffer;

public:
	ChttpSessionMan();
	int maxSessionIdle;
	unsigned int maxSessions;
	unsigned int maxSessionQueue;

	//Called from http-client-side:
	void getJsonQueue(int netId, ThttpCookie & authCookie, string & jsonStr);
	string sendMessage(ThttpCookie & authCookie, string & jsonStr);
	void endGet(int netId, ThttpCookie & authCookie);

/*	int getSessionId(ThttpCookie httpCookie);
	bool isSessionValid(ThttpCookie httpCookie);*/
// 	void readyLongpoll(ThttpCookie httpCookie, int netId);
// 	void disconnected(int netId);
		
	//Called from synapse-side:
	void sessionStart(Cmsg & msg);
	void newSessionError(Cmsg & msg);
	void sessionEnd(Cmsg & msg);
	int enqueueMessage(Cmsg & msg, int dst);


private:
	//private stuff, no mutex locking!
	ChttpSessionMap::iterator findSessionByCookie(ThttpCookie & authCookie);

	void expireCheck(ChttpSessionMap::iterator httpSessionI);
	void expireCheckAll();

};

#endif
/** 

httpSessionMan is thread safe.
httpSession is NOT thread safe.


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




class ChttpSessionMan
{
	typedef map<int, ChttpSession> ChttpSessionMap;
	ChttpSessionMap httpSessionMap;

	mutex threadMutex;
 	struct drand48_data randomBuffer;

public:
	ChttpSessionMan();

	//Called from http-client-side:
	void getJsonQueue(int netId, ThttpCookie & authCookie, string & jsonStr);

/*	int getSessionId(ThttpCookie httpCookie);
	bool isSessionValid(ThttpCookie httpCookie);*/
// 	void readyLongpoll(ThttpCookie httpCookie, int netId);
// 	void disconnected(int netId);
		
	//Called from synapse-side:
	void sessionStart(Cmsg & msg);
	void newSessionError(Cmsg & msg);
	void sessionEnd(Cmsg & msg);
	int enqueueMessage(Cmsg & msg, int dst);



};

#endif
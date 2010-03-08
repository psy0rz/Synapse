/** Http session management

	-a http-client connects and does some kind of request
	-if client has no httpSession cookie yet, it will get one
	-If type of request:
		-is a GET/POST to normal urls:
			-will be handled in a standard way. (e.g. files will be served)
		-is a GET to the special /synapse url will trigger an event poll via the readyLongpoll() or alike function:
			-If the httpSession is new and no session has been requested yet, a core_NewSession will be sent to the core.
				-If the core replys with:
					-a module_NewSession_Error -> newSessionError will pass the error to the http-client. NOTE: this error was actually meant for http_json, but we have to inform the client somehow, right?
					-a module_SessionStart -> sessionStart will connect the synapse session id with the oldest http-session that has an outstanding request. 
			-From now on, all received events will be passed to the corresponding http-session. If the session has no ready longpoll, the message will be queued. 
			-If one http-session has multiple network connections, (because of multple open browser windows?), the message will be sent to all those connections.
		-is a POST to the special /synapse url:
			-the msg.src will be verfied against the http-session
			-the message will be send out to synapse.




*/
#ifndef CHTTPSESSIONMAN_H
#define CHTTPSESSIONMAN_H

#include <map>
#include <stdlib.h>
#include <string>
#include "chttpsession.h"
#include <boost/thread/mutex.hpp>

using namespace boost;

class ChttpSessionMan
{
	map<int, ChttpSession> httpSessionMap;
	mutex threadMutex;
 	struct drand48_data randomBuffer;

public:
	ChttpSessionMan();
    string generateCookie();
		
	//Called from synapse-side:
	void sessionStart(Cmsg & msg);
	void newSessionError(Cmsg & msg);
	void sendMessage(Cmsg & msg);

	//Called from http-client-side:
	void readyLongpoll(string cookie, int netId);
	void disconnected(int netId);


};

#endif
/** Http session management
	We have 3 things we need to track:
	-The synapse sessions
	-The http sessions (cookies)
	-The network sessions. (network connections)

	Converting synapse events to http events:
	-A synapse session resolves only to one cookie and one special network connection that can recieve events. If that connection is broken, events are lost.
	Converting http events to synapse events:
	-A http session can have multiple synapse/network sessions. Since a json event already contains the src-session id, we can just look up that sessionId and compare the cookie for validation.
*/

#include <map>

class ChttpSessionMan
{
	map<int, ChttpSession> httpSessionMap;
	mutex threadMutex;


public:
    void ChttpSessionMan();
	
	//set/get the networkId that should receive events for sessionId
    int getNetworkId(int sessionId);
    int setNetworkId(int sessionId, int networkId);

	//create a new random http session cookie
    string newHttpSession();

	//add a new synapse session
	void addSession(int sessionId)


};

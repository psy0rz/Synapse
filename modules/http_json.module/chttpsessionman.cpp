#include <time.h>
#include "chttpsessionman.h"
#include <sstream>

using namespace std;

ChttpSessionMan::ChttpSessionMan()
{
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

string ChttpSessionMan::newHttpSession()
{
	//FIXME: better random cookies?
	lock_guard<mutex> lock(threadMutex);
	stringstream httpSessionId;
	double rndNumber;
	drand48_r(&randomBuffer, &rndNumber);
	httpSessionId << rndNumber;

	//add to session map
	//TODO: cleanup old or unused sessions
	httpSessionMap[httpSessionId.str(s)]

	return (httpSessionId.str());
}

void ChttpSessionMan::readyLongpoll(string httpSessionId, int netId)
{
	lock_guard<mutex> lock(threadMutex);
	ChttpSessionMap::iterator httpSessionI=httpSessionMap.find(httpSessionId);

	//session already known?
	if (httpSessionI!=httpSessionMap.end())
	{

	}

}


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

string ChttpSessionMan::generateCookie()
{
	//FIXME: better random cookies?
	lock_guard<mutex> lock(threadMutex);
	stringstream s;
	double rndNumber;
	drand48_r(&randomBuffer, &rndNumber);
	s << rndNumber;
	return (s.str());
}

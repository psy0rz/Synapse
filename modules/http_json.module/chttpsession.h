
#ifndef CHTTPSESSION_H
#define CHTTPSESSION_H

#include <string>

typedef long int ThttpCookie;


using namespace std;


class ChttpSession
{
	public:
	ThttpCookie authCookie;
	string jsonQueue;
	int netId;
	time_t lastTime;
	bool expired;

	ChttpSession();

	//admin/debugging
	string getStatusStr();	

};


#endif
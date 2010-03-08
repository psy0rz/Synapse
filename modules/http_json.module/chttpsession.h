
#ifndef CHTTPSESSION_H
#define CHTTPSESSION_H

#include <string>

using namespace std;

class ChttpSession
{
	string cookie;
	//only one network-connection can receive synapse events. 
	int recvNetId;
	ChttpSession();

};

#endif
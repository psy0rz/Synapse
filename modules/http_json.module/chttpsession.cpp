#include "chttpsession.h"

#include "../../csession.h"

#include "synapse_json.h"

ChttpSession::ChttpSession()
{
	netId=0;
	lastTime=time(NULL);
	expired=false;
}


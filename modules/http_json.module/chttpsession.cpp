#include "chttpsession.h"

#include "../../csession.h"

#include "synapse_json.h"

ChttpSession::ChttpSession()
{
	netId=0;
	lastTime=time(NULL);
	expired=false;
}

string ChttpSession::getStatusStr()
{	
	stringstream s;
	
	if (expired)
		s << "EXPIRED";
	else
		s << "ACTIVE";

	s << ", seen " << time(NULL)-lastTime << " seconds ago.";

	if (authCookie)
		s << " authCookie=" << authCookie;


	if (jsonQueue.length())
		s << " queue length=" << jsonQueue.length() << " bytes.";

	if (netId)
		s << " possible waiting netId=" << netId;


	return(s.str());
}

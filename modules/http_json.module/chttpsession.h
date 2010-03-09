
#ifndef CHTTPSESSION_H
#define CHTTPSESSION_H

#include <string>
#include <list>
#include <boost/shared_ptr.hpp>



using namespace std;
using namespace boost;


class ChttpSession
{
	public:

	//currently a httpSession can only be linked to ONE synapse session. If we need multi session support we can implement it later.
	int sessionId;
	ChttpSession();

};

typedef shared_ptr<ChttpSession> ChttpSessionPtr;

#endif
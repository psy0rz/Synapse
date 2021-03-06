/*  Copyright 2008,2009,2010 Edwin Eefting (edwin@datux.nl) 

    This file is part of Synapse.

    Synapse is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Synapse is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Synapse.  If not, see <http://www.gnu.org/licenses/>. */













#ifndef CMODULE_H
#define CMODULE_H


#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
//#include <hash_map.h>
#include "common.h"

namespace synapse
{
	using namespace boost;
	using namespace std;

	typedef boost::shared_ptr<class Cmodule> CmodulePtr;
}

#include "cmsg.h"
#include "csession.h"

namespace synapse
{
using namespace boost;
using namespace std;

//MODULE STUFF
#define SYNAPSE_API_VERSION 2

//defition of the default handler functions that modules have:
//TODO: can we optimize this be preventing the copy of msg? by using something const Cmsg & msg perhaps? we tried this but then msg["bla"] gave a compiler error.
#define SYNAPSE_HANDLER(s) extern "C" void synapse_##s( Cmsg msg, int dst, int cookie )

typedef  void (*FsoHandler) ( Cmsg msg , int dst, int cookie );
typedef  int (*FsoVersion)();
typedef bool (*FsoInit)(class CmessageMan * initMessageMan, CmodulePtr initModule);
typedef void (*FsoCleanup)();


//TODO: we COULD do this with a hash map, but stl doesnt has a stable good one and boost only has Unordered from 1.36 or higher. but its fast enough for now. (tested with 10000)
typedef map<string, FsoHandler> ChandlerHashMap;



/**
	@author 
*/
class Cmodule{
public:
	Cmodule();
	
	~Cmodule();
	void endThread();
	bool startThread();
	bool load(string name);
	bool unload();
	int maxThreads;
	int currentThreads;
	bool broadcastMulti; //send broadcasts to all sessions of every module, instead to the default session only
	bool broadcastCookie;//send broadcasts to each uniq session cookie
	int defaultSessionId;
	string name;
	string path;

	/* indicating when a module is ready is a tricky thing. Here is how it works:
		-a session calls core_LoadModule to load a module.
		-the session requesting the load is added to the requestingSessions list
		-the loading and initalizing of the module can take any amount of time.
		-other sessions that also call core_LoadModule in the meantime are added to the list as well.
		-as soon as the module sends out a core_Ready event, all the requesting sessions are informed with a modulename_Ready event.
		-the session id of the session that sended the core_LoadModule is stored in readySession. This also indicates the module is ready for future references.
		-if after this point someone does another core_LoadModule, they get back a modulename_Ready event instantly.
		 (they are also added to the requestingSessions for completeness sake, for now)

	*/
	int readySession;
	list<int> requestingSessions;

	void getEvents(Cvar & var);

	//event handling stuff,
	//converting an event string into an actual function call.
	bool setHandler(const string & eventName, const string & functionName);
	bool setDefaultHandler(const string & functionName);
	FsoHandler getHandler(string eventName);
	void * resolve(const string & functionName);
    bool isLoaded(string path);
    string getName(string path);
	FsoInit soInit;
	void *soHandle;

	private:
	ChandlerHashMap handlers;
	FsoHandler soDefaultHandler;


};

}

#endif

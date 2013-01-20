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













#ifndef CMESSAGEMAN_H
#define CMESSAGEMAN_H


#include <string>
#include <map>
#include "cevent.h"
#include "common.h"
//#include <hash_map.h>
#include "cuserman.h"
//#include "cmodule.h"
#include "cmsg.h"
#include "ccallman.h"
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/tss.hpp>

namespace synapse
{

/*
	This is the main object.

	Event marshhalling happens in two places:

	first to get the global Cevent object to verify the permissions:
	CmessageMan::CeventHashMap["eventname"] -- shared_ptr --> Cevent object

	secondly to get a pointer to the actual handler, per module:
	Cmodule::ChandlerHashMap["eventname"]   -- pointer --> soHandler() function



*/


using namespace std;
using namespace boost;

/**
	@author 
*/



class CmessageMan{
public:
	CmessageMan();
	
	~CmessageMan();

	//these 2 are 'the' threads, and do their own locking:
	void operator()();
	int run(string coreName,string moduleName);

	//the rest is not thread safe, so callers are responsible for locking:
	void sendMappedMessage(const CmodulePtr & modulePtr, const CmsgPtr & msg,int cookie);
	void sendMessage(const CmodulePtr & modulePtr, const CmsgPtr & msg, int cookie=0);

	void checkThread();

	CeventPtr getEvent(const string & name, const CuserPtr & user);
	void getEvents(Cvar & var);
    void doShutdown(int exit);

   	CsessionPtr loadModule(string path, string userName);
    CmodulePtr getModule(string path);

	mutex threadMutex;
	CuserMan userMan;
	CcallMan callMan;

	bool logSends;
	bool logReceives;

	//initial module that user want to start, after the coremodule is started:
	string firstModuleName;

	//for administrator/debugging
	void getStatus(Cvar & var);

	void setModuleThreads(CmodulePtr module, int maxThreads);
	void setSessionThreads(CsessionPtr session, int maxThreads);

	string addMapping(string mapFrom, string mapTo);
	string delMapping(string mapFrom, string mapTo);
	void getMapping(string mapFrom, Cvar & var);
private:
	condition_variable threadCond;


	//TODO: we COULD do this with a hash map, but stl doesnt has a good one and boost only has Unordered from 1.36 or higher. but its fast enough for now. (tested with 10000)
	typedef map<string, CeventPtr> CeventHashMap;
	CeventHashMap events;

	//event mapping: send another event as well. Usefull for key-bindings or lirc-bindings.
	typedef map<string, list<string> > CeventMapperMap;
	CeventMapperMap eventMappers;

	map<thread::id,CthreadPtr> threadMap;
	int 	currentThreads; //number of  threads in memory
	int	wantCurrentThreads;	//number of threads we want. threads will be added/deleted accordingly.
	int 	maxThreads;     //max number of threads. system will never create more then this many 
	int	    activeThreads;	//threads that are actually executing actively
	int 	maxActiveThreads; //max number of active threads seen (reset every X seconds by reaper)
	int     statMaxThreads; //maximum number of threads ever reached, just for statistics
	bool    shutdown; //program shutdown: end all threads
	int 	exit;     //shutdown exit code	

	bool idleThread();
	void activeThread();

	
	CgroupPtr defaultRecvGroup;
	CgroupPtr defaultSendGroup;
	CgroupPtr defaultModifyGroup;

	int statSends;
};

}

#endif

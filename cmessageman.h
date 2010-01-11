//
// C++ Interface: cmessageman
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CMESSAGEMAN_H
#define CMESSAGEMAN_H


#include <string>
#include <map>
#include "cevent.h"
#include "common.h"
#include <hash_map.h>
#include "cuserman.h"
//#include "cmodule.h"
#include "cmsg.h"
#include "ccallman.h"
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/recursive_mutex.hpp>

/*
	This is the main object.
	All the thread magic and locking happens in here.
	The other classes are not threadsafe!
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
	bool sendMessage(const CmodulePtr & modulePtr, const CmsgPtr & msg);

	void checkThread();
   	CsessionPtr loadModule(string path, string userName);
	CeventPtr getEvent(const string & name);
    int isModuleReady(string path);
    void doShutdown(int exit);

	mutex threadMutex;
	CuserMan userMan;
	CcallMan callMan;

	bool logSends;
	bool logReceives;

	//initial module that user want to start, after the coremodule is started:
	string firstModuleName;

private:
	condition_variable threadCond;


	//TODO: we COULD do this with a hash map, but stl doesnt has a good one and boost only has Unordered from 1.36 or higher. but its fast enough for now. (tested with 10000)
	typedef map<string, CeventPtr> CeventHashMap;
	CeventHashMap events;


	map<thread::id,CthreadPtr> threadMap;
	int 	currentThreads; //number of  threads in memory
	int	wantCurrentThreads;	//number of threads we want. threads will be added/deleted accordingly.
	int 	maxThreads;     //max number of threads. system will never create more then this many 
	int	activeThreads;	//threads that are actually executing actively
	int 	maxActiveThreads; //max number of active threads seen (reset every X seconds by reaper)
	bool    shutdown; //program shutdown: end all threads
	int 	exit;     //shutdown exit code	

	bool idleThread();
	void activeThread();

	CgroupPtr defaultRecvGroup;
	CgroupPtr defaultSendGroup;
	CgroupPtr defaultModifyGroup;
	//CuserPtr  defaultOwner;

};



#endif

//
// C++ Interface: cmodule
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CMODULE_H
#define CMODULE_H

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <hash_map.h>
#include "common.h"

using namespace boost;
using namespace std;

typedef shared_ptr<class Cmodule> CmodulePtr;

#include "cmsg.h"
#include "csession.h"


//MODULE STUFF
#define SYNAPSE_API_VERSION 2

//defition of the default handler functions that modules have:
//TODO: can we optimize this be preventing the copy of msg? by using something const Cmsg & msg perhaps? we tried this but then msg["bla"] gave a compiler error.
#define SYNAPSE_HANDLER(s) extern "C" void synapse_##s( Cmsg msg, int dst )

typedef  void (*FsoHandler) ( Cmsg msg , int dst );
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
	bool broadcastMulti;
	int defaultSessionId;
	string name;
	string path;
	bool ready;

	//event handling stuff,
	//converting an event string into an actual function call.
	FsoHandler soDefaultHandler;
	bool setHandler(const string & eventName, const string & functionName);
	FsoHandler getHandler(string eventName);
	void * resolve(const string & functionName);
    bool isLoaded(string path);
    string getName(string path);
	FsoInit soInit;
	void *soHandle;
	ChandlerHashMap handlers;


};



#endif

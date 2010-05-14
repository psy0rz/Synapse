//this header file includes all the headers that are needed to compile a synapse module
//TODO: split this up in 2 header files: one for the module init stuff and another thats a real header that can be included multiple times.


#include "cmsg.h"
#include "clog.h"
#include "cmodule.h"
#include "cmessageman.h"
#include <boost/foreach.hpp>
#include <stdarg.h>


#ifdef SYNAPSE_HAS_INIT
	void init();
#endif

//basic objects that are needed to call send messages
weak_ptr<Cmodule> module;  //weak pointer, otherwise the reference count would be of by one when unloading a module.
CmessageMan * messageMan; 

// Thanks to hkaiser@#boost for helping me with the automatic handler registration:
struct synapseAutoReg
{
    synapseAutoReg(char const* name)
    {
        handlers.push_back(name);
    }
    static list<string> handlers;
};

list<string> synapseAutoReg::handlers;

//This stores the handler in a list by constructing a dummy object:
#define SYNAPSE_REGISTER(name) \
    synapseAutoReg BOOST_PP_CAT(synapseAutoRegDummy, __LINE__)(#name); \
    SYNAPSE_HANDLER(name)


//to determine compatability of module:
extern "C" int synapseVersion() {
	return (SYNAPSE_API_VERSION); 
};
 



extern "C" bool synapseInit(CmessageMan * initMessageMan, CmodulePtr initModule)
{ 
	messageMan=initMessageMan;
	module=initModule;

	//when this function is called we have a core-lock, so we can register our handlers directly:
	BOOST_FOREACH(string handler, synapseAutoReg::handlers)
	{
		((CmodulePtr)module)->setHandler(handler, handler);
	}
	synapseAutoReg::handlers.clear();

	//this module has its own init function?
	#ifdef SYNAPSE_HAS_INIT
	init();
	#endif
	return true;
}

extern "C" void synapseCleanup()
{ 
	//nothing to do yet..
}
 


//make a copy of the message (can expensive for complex message structures)
//and send a smartpointer of it to the core.
//sendPtr should be defined in each module in synapse.h
//you CANT send messages from the core with this funtion, you'll get an 'undefined reference'. :)
bool Cmsg::send() 
{ 
	lock_guard<mutex> lock(messageMan->threadMutex);

	return(messageMan->sendMessage((CmodulePtr)module,CmsgPtr(new Cmsg(*this)))); 
} 


/*!
    \fn Cmsg::returnError(string error)
 */
void Cmsg::returnError(string description)
{
	ERROR("while handling " << event << ": " << description);
	Cmsg error;
	error.event="module_Error";
	error.dst=src;
	error.src=dst;
	error["event"]=event;
	error["description"]=description;
	error["parameters"]=*this;
	error.send();
}

bool Cmsg::returnIfOtherThan(char * keys, ...)
{
	va_list args;
	va_start(args,keys);
	
	char * key=keys;
	unsigned int found=0;
	
	do
	{		
		if (isSet(key))
		{
			found++;
		}
	}
	while ((key=va_arg(args,char *))!=NULL);

	va_end(args);

	if (((CvarMap)(*this)).size()>found)
	{
		returnError("Extra parameters found");
		return true;
	}
	return false;

}

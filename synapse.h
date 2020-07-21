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


//this header file includes all the headers that are needed to compile a synapse module
//TODO: split this up in 2 header files: one for the module init stuff and another thats a real header that can be included multiple times.


#include "cmsg.h"
#include "clog.h"
#include "cmodule.h"
#include "cmessageman.h"
#include <boost/foreach.hpp>
#include <stdarg.h>

namespace synapse
{


#ifdef SYNAPSE_HAS_INIT
	void init();
#endif

//basic objects that are needed to call send messages
boost::weak_ptr<Cmodule> module;  //weak pointer, otherwise the reference count would be of by one when unloading a module.
CmessageMan * messageMan;

// Thanks to hkaiser@#boost for helping me with the automatic handler registration:
struct synapseAutoReg
{
    synapseAutoReg(char const* name, string jsonStr)
    {
    	Cvar var;
    	if (jsonStr!="")
        {
            //strip begin and end quotes
            jsonStr=jsonStr.substr(1,jsonStr.length()-2);
    		var.fromJson(jsonStr);
        }
    	var["event"]=name;
        handlers.push_back(var);
    }

    static list<synapse::Cvar> handlers;
};

list<synapse::Cvar> synapseAutoReg::handlers;

//register a hanlder without specifying extra paraters:
#define SYNAPSE_REGISTER_1(name) \
    synapse::synapseAutoReg BOOST_PP_CAT(synapse_autoreg_, name)(#name, ""); \
    SYNAPSE_HANDLER(name)

//newer method to register handlers, this one uses an extra json string to specify many parameters of the event
//for example one could use:
//SYNAPSE_REGISTER(play_Pause, "{ 'recvGroup': 'everyone', 'sendGroup': 'module' }")

#define SYNAPSE_REGISTER_2(name, jsonVar) \
    synapse::synapseAutoReg BOOST_PP_CAT(synapse_autoreg_, name)(#name, #jsonVar); \
    SYNAPSE_HANDLER(name)

//some helper macros to choose the correct version of SYNAPSE_REGISTER, depending on the number of parameters
//thanks to http://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros
#define SYNAPSE_REGISTER_CHOOSER(x, A, B, FUNC, ...) FUNC
#define SYNAPSE_REGISTER(...) SYNAPSE_REGISTER_CHOOSER(, ##__VA_ARGS__, \
    												SYNAPSE_REGISTER_2(__VA_ARGS__), \
    												SYNAPSE_REGISTER_1(__VA_ARGS__)\
    												)

//to determine compatability of module:
extern "C" int synapseVersion() {
	return (SYNAPSE_API_VERSION);
};




extern "C" bool synapseInit(CmessageMan * initMessageMan, CmodulePtr initModule)
{
	messageMan=initMessageMan;
	module=initModule;

	//when this function is called we have a core-lock, so we can register our handlers directly:
	BOOST_FOREACH(synapse::Cvar var, synapseAutoReg::handlers)
	{
		//we dont use core_Register to avoid chicken egg problems when loading the core module
		((CmodulePtr)module)->setHandler(var["event"], var["event"]);

		//are there extra event parameters specified?
		if (var.isSet("sendGroup"))
		{
			CmsgPtr out(new Cmsg);
			out->event="core_ChangeEvent";
			out->dst=1;
			*out=var;
			//dont call out.send, it will deadlock..
			messageMan->sendMessage((CmodulePtr)module, out, 0);
		}
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
void Cmsg::send(int cookie, ElogLevel logLevel)
{
	boost::lock_guard<boost::mutex> lock(messageMan->threadMutex);

	messageMan->sendMessage((CmodulePtr)module,CmsgPtr(new Cmsg(*this)), cookie, logLevel);
}


/*!
    \fn Cmsg::returnError(string error)
 */
void Cmsg::returnError(string description)
{
	if (description!="")
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

}

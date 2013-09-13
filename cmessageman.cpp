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













#include "cmessageman.h"
#include "cmsg.h"
#include "clog.h"
#include "ccall.h"
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>
#include <dlfcn.h>
#include "csession.h"
#include "cmodule.h"
#include <boost/foreach.hpp>
#include "cevent.h"
#include <string.h>
#include <map>
#include <set>
#include <boost/thread/condition.hpp>

#include "libs/exception/cexception.h"


namespace synapse
{
__thread int currentThreadDstId=0; 

//keep this many threads left, even though we dont need them right away.
//This is to prevent useless thread destruction/creation.
#define SPARE_THREADS 10

using namespace boost;

CmessageMan::CmessageMan()
{
    //pretent like the current thread is handling a call that was send to session id 1, 
    //so that msg.src automaticly get 1 when the core call send with msg.src=0 
    currentThreadDstId=1; 
	logSends=true;
	logReceives=true;
//	defaultOwner=userMan.getUser("module");
	defaultSendGroup=userMan.getGroup("modules");
	defaultModifyGroup=userMan.getGroup("modules");
	defaultRecvGroup=userMan.getGroup("everyone");

	statSends=0;

	statMaxThreads=0;
	activeThreads=0;
	currentThreads=0;
	maxActiveThreads=0;
	wantCurrentThreads=1;
	maxThreads=1000;
	shutdown=false;
}


CmessageMan::~CmessageMan()
{
}




void CmessageMan::sendMappedMessage(const CmodulePtr &module, const CmsgPtr &  msg, int cookie)
{
	// -module is a pointer thats set by the core and can be trusted
	// -msg is set by the user and only containts direct objects, and NO pointers. it cant be trusted yet!
	// -our job is to verify if everything is ok and populate the call queue
	// -internally the core only works with smartpointers, so most stuff thats not in msg will be a smartpointer.
    // -cookie is checked against the session cookie. if it doesnt match an error is thrown. (this makes creating network modules easier)

//edwin: why?
//	if (shutdown)
//		throw(runtime_error("Shutting down, ignored message"));

	//no src session specified means we use the session the original message was send to.
    //we do this be using the global currentThreadDstId which is thread-local storage and contains the dst-session of the current call.
	//NOTE: this is the only case where modify the actual msg object.
	if (!msg->src)
	{
		if (currentThreadDstId!=0)
		{
			msg->src=currentThreadDstId;
		}
		else
		{
			stringstream s;
			s << "send: module " << module->name << " want to send " << msg->event << " with msg.src=0 from an unknown thread. (specify msg.src to prevent this)";
			throw(synapse::runtime_error(s.str().c_str()));
		}
	}


	//resolve source session id to session a pointer
 	//(its mandatory to have a valid session when sending stuff!)
	CsessionPtr src;
	src=userMan.getSession(msg->src);
	if (!src)
	{
		//not found

		//during shutdown all sessions are gone, we dont want exceptions anymore.
		if (shutdown)
		{
			DEB("send: ignored message " << msg->event << " from module " << module->name << " because we're shutting down and session has already been deleted.");
			return;
		}

		stringstream s;
		s << "send: module " << module->name << " want to send " << msg->event << " from non-existing session " << msg->src;
		throw(synapse::runtime_error(s.str().c_str()));
	}

	//source session belongs to this module?
	if (src->module!=module)
	{
		//module is not the session owner. 
		stringstream s;
		s << "send: module " << module->name << " wants to send " << msg->event << " from session " << msg->src << ", but isnt the owner of this session.";
		throw(synapse::runtime_error(s.str().c_str()));
	}

	//this cookie matches with src session cookie?
	//cookies are just used to make the programming and security of network modules easier
	if (cookie!=0 && cookie!=src->cookie)
	{
		stringstream s;
		s << "send: module " << module->name << " tries to send from session " << msg->src << ", but cookie " << cookie << " doesnt match session cookie " << src->cookie;
		throw(synapse::runtime_error(s.str().c_str()));
	}

	//resolve or create the event and check send-permissions:
	CeventPtr event=getEvent(msg->event, src->user);
	if (!event)
	{
		stringstream s;
		s << "send: session " << msg->src << " with user " << src->user->getName() << " is not allowed to send event " << msg->event;
		throw(synapse::runtime_error(s.str().c_str()));
	}

	src->statSends++;
	statSends++;

	stringstream msgStr;
	if (logSends)
	{
		//build a text-representaion of whats happening:
		msgStr << "SEND " << msg->event <<
			" FROM " << msg->src << ":" << src->user->getName() << "@" << module->name <<
			" TO ";
	}


	//check if destination(s) are allowed to RECEIVE:
	FsoHandler soHandler;

	//destination specified:
	if (msg->dst>0)
	{
		CsessionPtr dst;
		dst=userMan.getSession(msg->dst);
		//found it?
		if (!dst)
		{
			stringstream s;
			s << "send: destination session " << msg->dst << " not found";
			throw(synapse::runtime_error(s.str().c_str()));
		}

		//get the handler, and does it exist?
		soHandler=dst->module->getHandler(msg->event);
		if (soHandler==NULL)
		{
			WARNING("send ignored message: no handler for " << msg->event << " found in " << dst->module->name );
			return;
		}

		//is specified destination allowed?
		if (!event->isRecvAllowed(dst->user))
		{
			stringstream s;
			s <<  "send: session " << msg->dst << " with user " << dst->user->getName() << " is not allowed to receive event " << msg->event;
			throw(synapse::runtime_error(s.str().c_str()));
		}

		if (logSends)
		{
			msgStr << msg->dst << ":" << dst->user->getName() << "@" << dst->module->name <<
				" " << msg->getPrint(" |");
			LOG_SEND(msgStr.str());
		}

		//all checks ok:
		//make a copy of the message and add the destionation + handler to the call queue
		callMan.addCall(msg, dst, soHandler);

		//wake up a thread that calls the actual handler
		threadCond.notify_one();


		return;
	}
	//destination <=0 == broadcast
	else
	{
		if (logSends)
		{
			msgStr << "broadcast (";
		}


		//TODO:optimize these broadcasting algoritms
		CsessionPtr dst;
		bool delivered=false;
		static map <string,set<int> > sendedToCookie; //static for performance reasons.
		sendedToCookie.clear();
		for (int sessionId=0; sessionId<MAX_SESSIONS; sessionId++)
		{
			dst=userMan.getSession(sessionId);
			//session exists and is still active?
			if (dst && dst->isEnabled())
			{
				//session owner is allowed to receive the event?
				if (event->isRecvAllowed(dst->user))
				{
					//modules always receive the event on the defaultsession
					//OR on all sessions when broadcastMulti is true
					//OR send it to each uniq cookie. (more difficult and slow since we have to keep a list)
					if (dst->module->defaultSessionId==sessionId || dst->module->broadcastMulti ||
						( dst->module->broadcastCookie && sendedToCookie[dst->module->name].find(dst->cookie) == sendedToCookie[dst->module->name].end())
					)
					{
						//get the handler, and does it exist?
						soHandler=dst->module->getHandler(msg->event);
						if (soHandler!=NULL)
						{
							if (logSends)
								msgStr << dst->id << ":" << dst->user->getName() << "@" << dst->module->name << " ";


							callMan.addCall(msg, dst, soHandler);
							threadCond.notify_one();
							delivered=true;
							sendedToCookie[dst->module->name].insert(dst->cookie);

						}
					}
				}
			}
		}
		if (logSends)
		{
			msgStr << ") " <<
				msg->getPrint(" |");
			LOG_SEND(msgStr.str());
		}

		if (!delivered)
			WARNING("broadcast " << msg->event << " was not received by anyone.");
	}
}

/** Use this to send a message.
Internally it will result in 1 or more calls to sendMappedMessage, if the msg.dst is -1.
*/

void CmessageMan::sendMessage(const CmodulePtr &module, const CmsgPtr &  msg, int cookie)
{
	if (msg->dst >= 0)
	{
		sendMappedMessage(module, msg, cookie);
	}
	else
	{
		//TODO: move mapping functionality back to modules. current implementation is kind of a hack and not compatible with future expantions in the no-broadcast branch.
		//when we're done send a special mapping message that shows us what is mapped.
		//used by the mapper GUI.
		CmsgPtr mappedMsg=CmsgPtr(new Cmsg());
		mappedMsg->event="core_MappedEvent";
		//create or find the event in the mapper list, and traverse the list
		BOOST_FOREACH(string event, eventMappers[msg->event])
		{
			//clone the message and change the event-name
			CmsgPtr mapMsg=CmsgPtr(new Cmsg(*msg));
			(*mapMsg)["synapse_mappedFrom"]=msg->event; //long synapse-name, since we dont want it to collide with the original parameters of the message.
			(*mappedMsg)["mappedTo"].list().push_back(event);
			mapMsg->event=event;
			mapMsg->dst=0;
			sendMessage(module, mapMsg, cookie);
		}

		(*mappedMsg)["mappedFrom"]=msg->event;
		sendMessage(module, mappedMsg, cookie);

	}
}

void CmessageMan::operator()()
{
	CthreadPtr threadPtr;
	//init thread
	{
		lock_guard<mutex> lock(threadMutex);
		DEB("thread started");
		activeThread();
		//get a pointer to our own thread object and remove it from the threadmap
		threadPtr=threadMap[this_thread::get_id()];
		threadMap.erase(this_thread::get_id());
	}

	CcallList::iterator callI;

	//thread main-loop
	while(1)
	{
		//locking and call getting stuff...
		{
			//no interrupts here
			boost::this_thread::disable_interruption di;

			//lock core
			unique_lock<mutex> lock(threadMutex);


			//end previous call
			if (callI!=CcallList::iterator())
			{
				callMan.endCall(callI);
			}

			//get next call
			while ((callI=callMan.startCall(threadPtr)) == CcallList::iterator())
			{
				//no call ready...
				//indicate we're idle
				if (!idleThread())
				{
					//we need to die :(
					DEB("thread ending");
					return;
				}
				//unlock+sleep until we get signalled
				threadCond.wait(lock);

				//indicate we're active
				activeThread();
			}

			//check if we need more threads
			checkThread();

		} //unlock core

		if (logReceives)
		{

			stringstream msgStr;
			msgStr << "RECV " << callI->msg->event <<
				" FROM " << callI->msg->src <<
				" BY " << callI->dst->id << ":" << callI->dst->user->getName() << "@" << callI->dst->module->name << " " <<
				callI->msg->getPrint(" |");

			LOG_RECV(msgStr.str());
		}

		//handle call
		try
		{
            //this is used in sendmessage to automagically determine the source session when its not specified.
            currentThreadDstId=callI->dst->id;
			callI->soHandler(*(callI->msg), callI->dst->id, callI->dst->cookie);
			currentThreadDstId=1;
		}
	  	catch (const ios::failure& e)
  		{
			//return std::exceptions as error events
			lock_guard<mutex> lock(threadMutex);

			ERROR("I/O error while handling " << callI->msg->event << ": " << strerror(errno));
			CmsgPtr error(new Cmsg);
			(*error).event="module_Error";
			(*error).dst=callI->msg->src;
			(*error).src=callI->dst->id;
			(*error)["event"]=callI->msg->event;
			(*error)["description"]="I/O error: " + string(strerror(errno));
			(*error)["parameters"]=(*callI->msg);
			//prevent exception loops
			try
			{
				sendMessage(callI->dst->module, error);
			}
			catch(...){};
		}
	  	catch (const synapse::runtime_error& e)
  		{
			//return std::exceptions as error events
			lock_guard<mutex> lock(threadMutex);

			ERROR("synapse exception while handling " << callI->msg->event << ": " << e.what());
			DEB("Backtrace:\n" << e.getTrace());

			CmsgPtr error(new Cmsg);
			(*error).event="module_Error";
			(*error).dst=callI->msg->src;
			(*error).src=callI->dst->id;
			(*error)["event"]=callI->msg->event;
			(*error)["description"]="Exception: " + string(e.what());
			(*error)["parameters"]=(*callI->msg);
			try
			{
				sendMessage(callI->dst->module, error);
			}
			catch(...){};
		}
	  	catch (const std::exception& e)
  		{
			//return std::exceptions as error events
			lock_guard<mutex> lock(threadMutex);

			ERROR("exception while handling " << callI->msg->event << ": " << e.what());
			CmsgPtr error(new Cmsg);
			(*error).event="module_Error";
			(*error).dst=callI->msg->src;
			(*error).src=callI->dst->id;
			(*error)["event"]=callI->msg->event;
			(*error)["description"]="Exception: " + string(e.what());
			(*error)["parameters"]=(*callI->msg);
			try
			{
				sendMessage(callI->dst->module, error);
			}
			catch(...){};
		}
		catch(...)
		{
			//return other exceptions as error events
			lock_guard<mutex> lock(threadMutex);

			ERROR("unknown exception while handling " << callI->msg->event);
			CmsgPtr error(new Cmsg);
			(*error).event="module_Error";
			(*error).dst=callI->msg->src;
			(*error).src=callI->dst->id;
			(*error)["event"]=callI->msg->event;
			(*error)["description"]="Unknown exception";
			(*error)["parameters"]=(*callI->msg);
			try
			{
				sendMessage(callI->dst->module, error);
			}
			catch(...){};
		}
	}
}

/*!
    \fn CmessageMan::startThread()
	called when thread is getting active
 */
void CmessageMan::activeThread()
{
	activeThreads++;
	//keep maximum active threads, so the reaper knows when there are too much threads
	if (activeThreads>maxActiveThreads)
	{
		maxActiveThreads=activeThreads;
		if (activeThreads>statMaxThreads)
			statMaxThreads=activeThreads;
	}
}



/*!
	called when thread is ready and does nothing
 */
bool CmessageMan::idleThread()
{
	activeThreads--;
	//we want less threads? let this one die by returning false
	if (wantCurrentThreads<currentThreads)
	{
		currentThreads--;
		return false;
	}
	return true;
}


/*!
    \fn CmessageMan::checkThread()
	called while thread is active to see if we need more threads
 */
void CmessageMan::checkThread()
{

	//if all threads are active, indicate that we want one want one more
	//(there always should be a least one idle thread)
	if (activeThreads>=wantCurrentThreads)
	{
		wantCurrentThreads=activeThreads+1;
	}

	//we want more threads? start another one
	while (wantCurrentThreads>currentThreads && currentThreads<maxThreads)
	{
		//then start a new thread
		DEB(activeThreads << "/" << currentThreads << " threads active, starting one more...");
		currentThreads++;

		CthreadPtr threadPtr(new thread(boost::ref(*this)));
		threadMap[threadPtr->get_id()]=threadPtr;
		//note: the new thread will execute this->operator()
	}
}




/*!
    \fn CmessageMan::run()
 */
int CmessageMan::run(string coreName, string moduleName)
{

	//load the first module as user core UNLOCKED!
	loadModule(getModulePath(coreName), "core");
	this->firstModuleName=moduleName;

	//start first thread:
	checkThread();

	//thread manager loop
	while (! (shutdown && callMan.callList.empty() ))
	{
		sleep(1);

		//lock core and do our stuff
		{
			lock_guard<mutex> lock(threadMutex);
// 			DEB(maxActiveThreads << "/" << wantCurrentThreads << " threads active.");
// 			callMan.print();
// 			userMan.print();

			if (maxActiveThreads+SPARE_THREADS+1 < wantCurrentThreads)
			{
				wantCurrentThreads--;
				DEB("maxActiveThreads was " << maxActiveThreads << " so deceasing wantCurrentThreads to " << wantCurrentThreads);
				threadCond.notify_one();

			}
			maxActiveThreads=activeThreads;
		}

	}
	//loop exits when shutdown=true and callist is empty.

	//shutdown loop
	wantCurrentThreads=0;
	while(1)
	{
		{
			lock_guard<mutex> lock(threadMutex);
			if (currentThreads)
			{
				INFO("shutting down - waiting for threads to end:" << currentThreads);
				//send all running calls an interrupt
				threadCond.notify_all();
			}
			else
			{
				break;
			}
		}
		sleep(1);
	}

	//FIXME: are all threads really ended? its possible they still exist for a very short time, after they decreased the currentThread-counter.
	sleep(1);
	INFO("all threads ended, exiting.");
	return(exit);
}


//determines the pathname of a module by name. (module doesnt have to be loaded yet)
string CmessageMan::getModulePath(string name)
{
	//TODO: make this configurable
	string path="modules/"+name+".module/lib"+name+".so";
	return (path);
}


/** Loads a module an returns a pointer to the newly created default-session for the module.
 * Only call this once!
 */
CsessionPtr CmessageMan::loadModule(string path, string userName)
{
	CmodulePtr module(new Cmodule);

	if (shutdown)
	{
		ERROR("Shutting down, cant load new module: " << path);
		return (CsessionPtr());
	}

	//modules get unloaded automaticly when the module-object is deleted:
	if (module->load(path))
	{
		CuserPtr user(userMan.getUser(userName));
		if (user)
		{
			//we need a session for the init function of the core-module:
			CsessionPtr session(new Csession(user,module));
			module->defaultSessionId=userMan.addSession(session);
			session->description="module default session.";

			DEB("Init module " << module->name);

            currentThreadDstId=module->defaultSessionId;
			if (module->soInit(this ,module))
			{
                currentThreadDstId=1;
				DEB("Init " << module->name << " complete");
				return session;
			}
            currentThreadDstId=0;
			userMan.delSession(module->defaultSessionId);
			ERROR("Error while initalizing module");
		}
	}

	return CsessionPtr();
}

/** Return a pointer to the specified module, if its loaded.
 */
CmodulePtr CmessageMan::getModule(string path)
{
	CsessionPtr session;
	string name;
	name=Cmodule().getName(path);

	for (int sessionId=0; sessionId<MAX_SESSIONS; sessionId++)
	{
		session=userMan.getSession(sessionId);
		//does the session point to the module we're looking for?
		if (session && session->module && session->module->name==name)
		{
			return (session->module);
		}
	}
	return CmodulePtr();
}


/*!
    \fn CmessageMan::getEvent(const & string name)
 */
CeventPtr CmessageMan::getEvent(const string & name, const CuserPtr & user)
{
	CeventHashMap::iterator eventI;
	eventI=events.find(name);
	//not found?
	if (eventI==events.end())
	{
		DEB("adding new event: " << name);
		CeventPtr event(new Cevent(defaultModifyGroup, defaultSendGroup, defaultRecvGroup));

		//if sending is not allowed, then dont add it to the list to prevent DOS attacks by anonymous users
		if (user && !event->isSendAllowed(user))
		{
			return (CeventPtr());
		}

		events[name]=event;
		return (event);
	}
	else
	{
		if (user && !eventI->second->isSendAllowed(user))
		{
			return (CeventPtr());
		}
		return eventI->second;
	}
}

void CmessageMan::getEvents(Cvar & var)
{
	//traverse the events that have been created by calls to sendMessage
	BOOST_FOREACH( CeventHashMap::value_type event, events)
	{
		var[event.first]=1;
	}

	//traverse the events that have registered event handlers in a module
	CsessionPtr session;
	for (int sessionId=0; sessionId<MAX_SESSIONS; sessionId++)
	{
		session=userMan.getSession(sessionId);
		//session exists and is still active?
		if (session && session->isEnabled())
		{
			//is this the default session for the module (to prevent overhead)
			if (session->module->defaultSessionId==sessionId)
			{
				session->module->getEvents(var);
			}
		}
	}


	//get more information for each event
	FOREACH_VARMAP_ITER(eventI,var)
	{
		string s=eventI->first;
		CeventPtr eventPtr=getEvent(s, CuserPtr());
		eventI->second["recvGroup"]=eventPtr->getRecvGroup()->getName();
		eventI->second["sendGroup"]=eventPtr->getSendGroup()->getName();
		eventI->second["modifyGroup"]=eventPtr->getModifyGroup()->getName();
	}
}




/*!
    \fn CmessageMan::doShutdown()
 */
void CmessageMan::doShutdown(int exit=0)
{
	WARNING("Shutdown requested, exit code="<<exit);
	shutdown=true;
	userMan.doShutdown();
	this->exit=exit;
	threadCond.notify_all();
}

void CmessageMan::getStatus(Cvar & var)
{
	var["activeThreads"]=activeThreads;
	var["currentThreads"]=currentThreads;
	var["statMaxThreads"]=statMaxThreads;
	var["statSends"]=statSends;

}


void CmessageMan::setModuleThreads(CmodulePtr module, int maxThreads)
{
	module->maxThreads=maxThreads;
	//TODO:optimize
	threadCond.notify_all();
}

void CmessageMan::setSessionThreads(CsessionPtr session, int maxThreads)
{
	session->maxThreads=maxThreads;
	//TODO:optimize
	threadCond.notify_all();

}

string CmessageMan::addMapping(string mapFrom, string mapTo)
{
	if (eventMappers.find(mapFrom)!=eventMappers.end())
	{
		if (find(eventMappers[mapFrom].begin(), eventMappers[mapFrom].end(), mapTo)!=eventMappers[mapFrom].end())
		{
			return("This mapping already exists!");
		}
	}

	eventMappers[mapFrom].push_back(mapTo);
	return ("");
}

string CmessageMan::delMapping(string mapFrom, string mapTo)
{
	if (eventMappers.find(mapFrom)!=eventMappers.end())
	{
		if (find(eventMappers[mapFrom].begin(), eventMappers[mapFrom].end(), mapTo)!=eventMappers[mapFrom].end())
		{
			eventMappers[mapFrom].remove(mapTo);
			return ("");
		}
	}

	return("Mapping does not exist!");
}

void CmessageMan::getMapping(string mapFrom, Cvar & var)
{
		if (eventMappers.find(mapFrom)==eventMappers.end())
			return;

		BOOST_FOREACH(string event, eventMappers[mapFrom])
		{
			var.list().push_back(event);
		}
}
}

//
// C++ Implementation: cmessageman
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
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

using namespace boost;

CmessageMan::CmessageMan()
{
	logSends=true;
	logReceives=true;
//	defaultOwner=userMan.getUser("module");
	defaultSendGroup=userMan.getGroup("modules");
	defaultModifyGroup=userMan.getGroup("modules");
	defaultRecvGroup=userMan.getGroup("everyone");

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




/*!
    \fn CmessageMan::sendMessage(const Cmsg & msg) */
bool CmessageMan::sendMessage(const CmodulePtr &module, const CmsgPtr &  msg)
{
	// -module is a pointer thats set by the core and can be trusted
	// -msg is set by the user and only containts direct objects, and NO pointers. it cant be trusted yet!
	// -our job is to verify if everything is ok and populate the call queue
	// -internally the core only works with smartpointers, so most stuff thats not in msg will be a smartpointer.

	if (shutdown)
		return false;

	//no src session specified means use default session of module:
	//NOTE: this is the only case where modify the actual msg object.
	if (!msg->src)
	{
		if (module->defaultSessionId!=SESSION_DISABLED)
		{
			msg->src=module->defaultSessionId;
		}
		else
		{
			ERROR("send: module " << module->name << " want to send " << msg->event << " from its default session, but is doesnt have one." );
			return false;
		}
	}


	//resolve source session id to session a pointer
 	//(its mandatory to have a valid session when sending stuff!)
	CsessionPtr src;
	src=userMan.getSession(msg->src);
	if (!src)
	{
		//not found. we cant send an error back yet, so just return false
		ERROR("send: module " << module->name << " want to send " << msg->event << " from non-existing session " << msg->src );
		return false;
	}

	//source session belongs to this module?
	if (src->module!=module)
	{
		//module is not the session owner. we cant send an error back yet, so just return false
		ERROR("send: module " << module->name << " wants to send " << msg->event << " from session " << msg->src << ", but isnt the owner of this session.");
		return false;
	}


	//resolve or create the event:
	CeventPtr event=getEvent(msg->event);

	//check if src is allowed to SEND:
	if (!event->isSendAllowed(src->user))
	{
		ERROR("send: session " << msg->src << " with user " << src->user->getName() << " is not allowed to send event " << msg->event);
		return false;
	}


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
	if (msg->dst!=0)
	{
		CsessionPtr dst;
		dst=userMan.getSession(msg->dst);
		//found it?
		if (!dst)
		{
			ERROR("send: destination session " << msg->dst << " not found");
			return false;
		}

		//get the handler, and does it exist?
		soHandler=dst->module->getHandler(msg->event);
		if (soHandler==NULL)
		{
			WARNING("send ignored message: no handler for " << msg->event << " found in " << dst->module->name );
			return false;
		}

		//is specified destination allowed?
		if (!event->isRecvAllowed(dst->user))
		{
			ERROR("send: session " << msg->dst << " with user " << dst->user->getName() << " is not allowed to receive event " << msg->event);
			return false;
		}

		if (logSends)
		{
			msgStr << msg->dst << ":" << dst->user->getName() << "@" << dst->module->name << 
				" DATA=" << msg->getPrint(" |");
			LOG_SEND(msgStr.str());
		}

		//all checks ok:
		//make a copy of the message and add the destionation + handler to the call queue
		callMan.addCall(msg, dst, soHandler);

		//wake up a thread that calls the actual handler
		threadCond.notify_one();


		return true;
	}
	//no destination specified == broadcast
	else
	{
		if (logSends)
		{
			msgStr << "broadcast (";
		}

		//make a copy of the message object and keep the pointer:
		CmsgPtr msgPtr(new Cmsg(*msg));
		//TODO:optimize these broadcasting algoritms
		CsessionPtr dst;
		bool delivered=false;
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
					if (dst->module->defaultSessionId==sessionId || dst->module->broadcastMulti)
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
						}
					}
				}
			}
		}
		if (logSends)
		{
			msgStr << ") DATA=" <<  
				msg->getPrint(" |");
			LOG_SEND(msgStr.str());
		}

		if (!delivered)
			WARNING("broadcast " << msg->event << " was not received by anyone.") 
		return (true);
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
				" BY " << callI->dst->id << ":" << callI->dst->user->getName() << "@" << callI->dst->module->name << " DATA=" <<
				callI->msg->getPrint(" |");

			LOG_RECV(msgStr.str());
		}

		//handle call
		try
		{
			callI->soHandler(*(callI->msg), callI->dst->id);
		}
	  	catch (std::exception& e)
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
			sendMessage(callI->dst->module, error); 
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
			sendMessage(callI->dst->module, error); 

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
		maxActiveThreads=activeThreads;

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
	loadModule(coreName, "core");
	this->firstModuleName=moduleName;

	//start first thread:
	checkThread();

	//thread manager loop
	while (! (shutdown && callMan.callList.empty() ))
	{
		sleep(10);

		//lock core and do our stuff
		{
			lock_guard<mutex> lock(threadMutex);
			DEB(maxActiveThreads << "/" << wantCurrentThreads << " threads active."); 
			callMan.print();
			userMan.print();

			if (maxActiveThreads<wantCurrentThreads-1)
			{
				wantCurrentThreads--;
				threadCond.notify_one();
				
				DEB("maxActiveThreads was " << maxActiveThreads << " so deceasing wantCurrentThreads to " << wantCurrentThreads);
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
	INFO("all threads ended, cleaning up...");
	return(exit);
}




/*!
    \fn CmessageMan::loadModule(string name)
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

			DEB("Init module " << module->name);
			if (module->soInit(this ,module))
			{
				DEB("Init " << module->name << " complete");
				return session;
			}
			userMan.delSession(module->defaultSessionId);
			ERROR("Error while initalizing module");
		}
	}

	return CsessionPtr();
}


/*!
    \fn CmessageMan::getEvent(const & string name)
 */
CeventPtr CmessageMan::getEvent(const string & name)
{
	CeventHashMap::iterator eventI;
	eventI=events.find(name);
	//not found?
	if (eventI==events.end())
	{
		DEB("adding new event: " << name);
		CeventPtr event(new Cevent(defaultModifyGroup, defaultSendGroup, defaultRecvGroup));
		events[name]=event;
		return (event);
	}
	else
	{
		return eventI->second;
	}
}





/*!
    \fn CmessageMan::isModuleReady(string name)
 */
int CmessageMan::isModuleReady(string path)
{
	CsessionPtr session;
	string name;
	name=Cmodule().getName(path);
	
	for (int sessionId=0; sessionId<MAX_SESSIONS; sessionId++)
	{
		session=userMan.getSession(sessionId);
		//session exists and is still active, and its the module?
		if (session && session->isEnabled() && session->module->name==name && session->module->readySession==session->id)
		{
			return (session->module->readySession);
		}
	}
	return SESSION_DISABLED;	
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

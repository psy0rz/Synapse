#define SYNAPSE_HAS_INIT
#include "synapse.h"
#include <signal.h>


//Dont forget you can only do things to core-objects after locking the core!
//Also dont forget you CANT send() while holding the core-lock. (it will deadlock)

//this is THE core module - we need a bootstrap function:
//(init is automagically called if it exists)
void init()
{
	//We have no core lock, but there are no threads yet, so its no problem.
	//What this means is that we can do everything we want AND use the send() without causing a deadlock.
	
	INFO("Synapse core v1.0 starting up...");

	//call the normal module-init to do the rest:
	Cmsg out;
	out.event="module_Init";
	out.send();

}
 
void exitHandler(int signum)
{
	//disable it from now on, so pressing ctrl-c a second time interrupts it.
	signal(SIGINT, SIG_DFL);

	Cmsg out;
	INFO("Interrupt received, calling shutdown (press ctrl-c again to stop now)");
	out.event="core_Shutdown";
	out.send();
}

SYNAPSE_REGISTER(module_Init)
{

	DEB("Core init start");
	Cmsg out;

	if (dst!=1)
	{
		ERROR("This core-module should be started as the first and only one.");
		return;
	}

	/// Shutdown signal handler
	signal(SIGINT, exitHandler);

	/// SET CORE EVENT PERMISSIONS:
	// these permissions should never be changed!
	out.clear();
	out.event="core_ChangeEvent";


	/// basic core events
	out["event"]=		"module_Error"; /// SEND to module on all kinds of errors
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"modules";
	out["recvGroup"]=	"everyone";
	out.send();

	out["event"]=		"core_ChangeEvent"; /// RECV to change permissions of events
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"modules";
	out["recvGroup"]=	"core";
	out.send();

	out["event"]=		"core_Register";  /// RECV to register handlers
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"modules";
	out["recvGroup"]=	"core";
	out.send();


	/// module loading and unloading
	out["event"]=		"core_LoadModule"; /// RECV to load a module
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"modules";
	out["recvGroup"]=	"core";
	out.send();

	out["event"]=		"module_Init";  /// SEND to module after loading it
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"core";
	out["recvGroup"]=	"modules";
	out.send();

	out["event"]=		"module_Shutdown"; /// SEND because we want to unload the module cleanup
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"core";
	out["recvGroup"]=	"modules";
	out.send();


	/// session handling
	out["event"]=		"core_NewSession"; /// RECV to start a new session
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"modules";
	out["recvGroup"]=	"core";
	out.send();

	out["event"]=		"module_SessionStart"; /// SEND to newly started session
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"core";
	out["recvGroup"]=	"anonymous";
	out.send();

	out["event"]=		"module_SessionStarted"; /// SEND to broadcast, on newly created session
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"core";
	out["recvGroup"]=	"everyone";
	out.send();

	out["event"]=		"core_DelSession"; /// RECV to delete src session
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"core";
	out.send();

	out["event"]=		"module_SessionEnd"; /// SEND to ended session
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"core";
	out["recvGroup"]=	"anonymous";
	out.send();

	out["event"]=		"module_SessionEnded"; /// SEND to broadcast, on ended session
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"core";
	out["recvGroup"]=	"everyone";
	out.send();

	out["event"]=		"core_Login"; /// RECV to login the src session with specified user and password
	out["modifyGroup"]=	"core";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"core";
	out.send();

	out["event"]="module_Login"; /// SEND to session after succesfull login
	out["modifyGroup"]="core";
	out["sendGroup"]="core";
	out["recvGroup"]="anonymous";
	out.send();

	out["event"]="core_ChangeSession"; /// RECV to change src session parameters
	out["modifyGroup"]="core";
	out["sendGroup"]="modules";
	out["recvGroup"]="core";
	out.send();


	/// END OF PERMISSIONS

	//we're done with our stuff,
	//load the first application module:
	out.clear();
	out.event="core_LoadModule";
	out["path"]=messageMan->firstModuleName;
	out.send();


	DEB("Core init complete");	
}


SYNAPSE_REGISTER(module_Error)
{
	//ignore errors for now..
}


SYNAPSE_REGISTER(core_LoadModule)
{
	string error;
	Cmsg out;
	{
		lock_guard<mutex> lock(messageMan->threadMutex);
		CsessionPtr session;
		Cmodule module;
	
		//its already loaded?
		if (module.isLoaded(msg["path"]))
		{
			DEB("module " << (string)msg["path"] << " is already loaded");
			//is it ready as well?
			int readySession=messageMan->isModuleReady(msg["path"]);
			if (readySession!=SESSION_DISABLED)
			{
				//send out a modulename_ready to the requesting session to inform the module is already ready:
				out.event=module.getName((string)msg["path"])+"_Ready";
				DEB("module is already ready, sending a " << out.event);
				out.dst=msg.src;
				out["session"]=readySession;
			}
			else
			{
				return;
			}
		}
		else
		{
			session=messageMan->loadModule(msg["path"],"module");
			if (!session)
				error="Error while loading module.";
			else
			{
				out.event="module_Init";
				out.dst=session->id;
			}
		}
	}

	if (error!="")
		msg.returnError(error);
	else
	{
		out.send();
	}
}



SYNAPSE_REGISTER(core_Register)
{
	string error;
	{
		lock_guard<mutex> lock(messageMan->threadMutex);

		CsessionPtr session=messageMan->userMan.getSession(msg.src);
		if (!session)
			error="Can't find session";
		else
		{
			string handler;
			if (msg.isSet("handler"))
				handler=(string)msg["handler"];
			else
				handler=(string)msg["event"];

			if (msg.isSet("event"))
			{
				//set a handler for a specific event
				if (!session->module->setHandler(msg["event"], handler))
					error="Can't find handler "+handler+" in module "+session->module->name;
				else
					return;
			}
			else
			{
				//set the default handler for all events
				if (!session->module->setDefaultHandler(handler))
					error="Can't find default handler "+handler+" in module "+session->module->name;
				else
					return;
			}
		}
	}
	msg.returnError(error);
}


SYNAPSE_REGISTER(core_ChangeEvent)
{
	//since permissions are very important, make sure the user didnt make a typo.
	//(normally we dont care about typo's since then something just wouldn't work, but this function will work if the user doesnt specify one or more parameters.)
	if (msg.returnIfOtherThan("sendGroup","event","recvGroup","modifyGroup",NULL))
		return;

	string error;
 	{
		lock_guard<mutex> lock(messageMan->threadMutex);
		CsessionPtr session=messageMan->userMan.getSession(msg.src);
		if (!session)
			error="Session not found";
		else
		{
			CeventPtr event=messageMan->getEvent(msg["event"]);
			if (!event)
				error="Event not found?";
			else
			{
				if (!event->isModifyAllowed(session->user))
					error="You're not allowed to modify this event.";
				else
				{
					CgroupPtr group;

					if (msg.isSet("modifyGroup"))
					{
						if (!(group=messageMan->userMan.getGroup(msg["modifyGroup"])))
							error+="Cant find modifygroup " +(string)msg["modifyGroup"]+ " ";
						else
							event->setModifyGroup(group);
					}

					if (msg.isSet("recvGroup"))
					{
						if (!(group=messageMan->userMan.getGroup(msg["recvGroup"])))
							error="Cant find recvgroup " + (string)msg["recvGroup"]+ " ";
						else
							event->setRecvGroup(group);
					}

					if (msg.isSet("sendGroup"))
					{
						if (!(group=messageMan->userMan.getGroup(msg["sendGroup"])))
							error+="Cant find sendgroup " + (string)msg["sendGroup"]+ " ";
						else
							event->setSendGroup(group);
					}
				}
			}
		}
	}

	if (error!="")
		msg.returnError(error);
} 

/** core_Login
 * Check username and password and changes users of src session.
 * Sends: module_Login to src
 */
SYNAPSE_REGISTER(core_Login)
{
	string error;
	Cmsg loginmsg;
	{
		lock_guard<mutex> lock(messageMan->threadMutex);
		error=messageMan->userMan.login(msg.src, msg["username"], msg["password"]);
		if (error=="")
		{
			//send login message to the session that was requesting the login
			loginmsg.event="module_Login";
			loginmsg.dst=msg.src;
			loginmsg["username"]=msg["username"];
		}
	}

	if (error!="")
		msg.returnError(error);
	else
	{
		loginmsg.send();
	}
}

/** core_NewSession
 * Starts new session with the src user as owner
 * Sends: 		module_SessionStart to new session 
 * Sends: 		module_SessionStarted to broadcast
 * On failure:	module_NewSession_Error
 */
SYNAPSE_REGISTER(core_NewSession)
{
	string error;
	Cmsg startmsg;

	{
		lock_guard<mutex> lock(messageMan->threadMutex);
		CsessionPtr session=messageMan->userMan.getSession(msg.src);
		if (!session)
			error="Session not found";
		else
		{
			CsessionPtr newSession=CsessionPtr(new Csession(session->user,session->module));
			int sessionId=messageMan->userMan.addSession(newSession);
			if (sessionId==SESSION_DISABLED)
				error="cant create new session";
			else
			{
				//login?
				if (msg.isSet("username"))
					error=messageMan->userMan.login(sessionId, msg["username"], msg["password"]);

				if (error!="")
				{
					//login failed, delete session again
					messageMan->userMan.delSession(msg.src);
				}
				{
					//set max threads?
					if (msg.isSet("maxThreads") && msg["maxThreads"] > 0)
						newSession->maxThreads=msg["maxThreads"];
	
					//send startmessage to the new session, copy all parameters.
					startmsg=msg;
					startmsg.event="module_SessionStart";
					startmsg.dst=sessionId;
					startmsg.src=0;
				}
			}
		}
	}

	if (error!="")
	{
		//return error to the requester
		Cmsg errormsg;
		errormsg=msg;
		errormsg.dst=msg.src;
		errormsg.src=0;
		errormsg.event="module_NewSession_Error";
		errormsg["error"]=error;
		errormsg.send();
	}
	else
	{
		startmsg.send();

		//also broadcast module_SessionStarted, so other modules know that a session is started
		startmsg.clear();
		startmsg.event="module_SessionStarted";
		startmsg["session"]=startmsg.dst;
		startmsg.dst=0;
		startmsg.send();
	}
}


SYNAPSE_REGISTER(core_Shutdown)
{
	//this ends all sessions, so that all modules are eventually unloaded and the core shuts down.

	Cmsg endmsg;
	lock_guard<mutex> lock(messageMan->threadMutex);

	WARNING("Shutdown requested, ending all sessions.");

	//first tell all the modules we want them to shut down
	endmsg.clear();
	endmsg.event="module_Shutdown";
	endmsg.dst=0;
	//use lowlevel sendMessage, since endmsg.send would deadlock
	messageMan->sendMessage((CmodulePtr)module,CmsgPtr(new Cmsg(endmsg))); 
	

	//now tell all the sessions they are ended, and actually delete them
	//do this PER session.
	for (int sessionId=2; sessionId<MAX_SESSIONS; sessionId++)
	{

		CsessionPtr session=messageMan->userMan.getSession(sessionId);
		if (session)
		{
			//send endmessage to the (still existing ) session:
			endmsg.clear();
			endmsg.event="module_SessionEnd";
			endmsg.dst=sessionId;
			//use lowlevel sendMessage, since endmsg.send would deadlock
			messageMan->sendMessage((CmodulePtr)module,CmsgPtr(new Cmsg(endmsg))); 
	
			//inform everyone the session has ended
			endmsg.clear();
			endmsg.event="module_SessionEnded";
			endmsg["session"]=sessionId;
			endmsg.dst=0;
			messageMan->sendMessage((CmodulePtr)module,CmsgPtr(new Cmsg(endmsg))); 
		
			//now actually delete the session
			//Csession object stays intact as long as there are shared_ptr's referring to it from the call queue
			if (!messageMan->userMan.delSession(sessionId))
					ERROR("cant delete session" << sessionId);
			
			//when the last session for a module is gone the module is unloaded.
			//when the last module is unloaded the program shuts down.
		}
	}
	
	//delete the core session, so nobody can do any corestuff from now on
	messageMan->userMan.delSession(1);

	//make the shutdown flag true:
	//-this prevents new sessions from being created and modules from being loaded.
	//-also all calls to sendMessage will be silently ignored.
	messageMan->doShutdown(msg["exit"]);
}


SYNAPSE_REGISTER(core_DelSession)
{
	string error;
	Cmsg endmsg;
	{
		lock_guard<mutex> lock(messageMan->threadMutex);
		CsessionPtr session=messageMan->userMan.getSession(msg.src);
		if (!session)
			error="Session not found";
	}

	if (error!="")
		msg.returnError(error);
	else
	{
		//send endmessage to the (still existing ) session:
		endmsg.event="module_SessionEnd";
		endmsg.dst=msg.src;
		endmsg.send();

		//inform the rest of the world
		endmsg.event="module_SessionEnded";
		endmsg["session"]=endmsg.dst;
		endmsg.dst=0;
		endmsg.send();
	
		//now actually delete the session
		{
			lock_guard<mutex> lock(messageMan->threadMutex);
			if (!messageMan->userMan.delSession(msg.src))
				ERROR("cant delete session" << msg.src);
		}
	}
}


SYNAPSE_REGISTER(core_ChangeModule)
{
	string error;
	{
		lock_guard<mutex> lock(messageMan->threadMutex);

		CsessionPtr session=messageMan->userMan.getSession(msg.src);
		if (!session)
			error="Can't find session";
		else
		{
			if (msg["maxThreads"] > 0)
				session->module->maxThreads=msg["maxThreads"];	
		}
	}
	if (error!="")
		msg.returnError(error);
}


SYNAPSE_REGISTER(core_ChangeSession)
{
	string error;
	{
		lock_guard<mutex> lock(messageMan->threadMutex);

		CsessionPtr session=messageMan->userMan.getSession(msg.src);
		if (!session)
			error="Can't find session";
		else
		{
			if (msg["maxThreads"] > 0)
				session->maxThreads=msg["maxThreads"];	
		}
	}
	if (error!="")
		msg.returnError(error);
}

//Indicates the module is ready with its init stuff.
//This results in a modulename_ready-broadcast to inform the other modules this one is ready to be used.
SYNAPSE_REGISTER(core_Ready)
{
	Cmsg out;
	string error;

	{
		lock_guard<mutex> lock(messageMan->threadMutex);

		CsessionPtr session=messageMan->userMan.getSession(msg.src);
		if (!session)
			error="Can't find session";
		else
		{
			out.event=session->module->name+"_Ready";
			out["session"]=session->id;;
			session->module->readySession=session->id;
		}
	}

	if (error!="")
		msg.returnError(error);
	else
		out.send();
}

//Sends a thread.interrupt() to a executing call, or removes it from the call queue if its not executing yet.
SYNAPSE_REGISTER(core_Interrupt)
{
	Cmsg out;
	string error;

	{
		lock_guard<mutex> lock(messageMan->threadMutex);
		if (!messageMan->callMan.interruptCall(msg["event"], msg.src, msg["dst"]))
			error="Cannot find call, or call has already ended";
		out.dst=msg.src;
		out.event="core_InterruptSent";
		out["pars"]=msg;
	}
	

	if (error!="")
		msg.returnError(error);
	else
		out.send();
}


SYNAPSE_REGISTER(core_ChangeLogging)
{
	{
		lock_guard<mutex> lock(messageMan->threadMutex);
		messageMan->logSends=msg["logSends"];
		messageMan->logReceives=msg["logReceives"];
	}
}

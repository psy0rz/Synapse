#define SYNAPSE_HAS_INIT
#include "synapse.h"

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
 

SYNAPSE_REGISTER(module_Init)
{

	DEB("Core init start");
	Cmsg out;

	if (dst!=1)
	{
		ERROR("This core-module should be started as the first and only one.");
		return;
	}


	///module_Error
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="module_Error";
	out["modifyGroup"]="core";
	out["sendGroup"]="modules";
	out["recvGroup"]="everyone";
	out.send();


	///set permissions on these important core features
	//The handlers where already registered by init()
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="core_ChangeEvent";
	out["modifyGroup"]="core";
	out["sendGroup"]="modules";
	out["recvGroup"]="core";
	out.send();

	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="core_Register";
	out["modifyGroup"]="core";
	out["sendGroup"]="modules";
	out["recvGroup"]="core";
	out.send();

	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="module_Init";
	out["modifyGroup"]="core";
	out["sendGroup"]="core";
	out["recvGroup"]="modules";
	out.send();

	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="core_LoadModule";
	out["modifyGroup"]="core";
	out["sendGroup"]="modules";
	out["recvGroup"]="core";
	out.send();


	/// core_Login
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="module_SessionStart";
	out["modifyGroup"]="core";
	out["sendGroup"]="core";
	out["recvGroup"]="everyone";
	out.send();

	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="module_SessionStarted";
	out["modifyGroup"]="core";
	out["sendGroup"]="core";
	out["recvGroup"]="everyone";
	out.send();

	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="module_Login";
	out["modifyGroup"]="core";
	out["sendGroup"]="core";
	out["recvGroup"]="modules";
	out.send();


	/// core_NewSession
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="core_NewSession";
	out["modifyGroup"]="core";
	out["sendGroup"]="modules";
	out["recvGroup"]="core";
	out.send();

	/// core_Logout
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="core_Logout";
	out["modifyGroup"]="core";
	out["sendGroup"]="everyone";
	out["recvGroup"]="core";
	out.send();

	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="module_SessionEnd";
	out["modifyGroup"]="core";
	out["sendGroup"]="core";
	out["recvGroup"]="everyone";
	out.send();

	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="module_SessionEnded";
	out["modifyGroup"]="core";
	out["sendGroup"]="core";
	out["recvGroup"]="everyone";
	out.send();

	/// core_ChangeModule
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="core_ChangeModule";
	out["modifyGroup"]="core";
	out["sendGroup"]="modules";
	out["recvGroup"]="core";
	out.send();

	/// core_ChangeSession
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="core_ChangeSession";
	out["modifyGroup"]="core";
	out["sendGroup"]="modules";
	out["recvGroup"]="core";
	out.send();

	/// core_Ready
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]="core_Ready";
	out["modifyGroup"]="core";
	out["sendGroup"]="modules";
	out["recvGroup"]="core";
	out.send();



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
			if (!messageMan->isModuleReady(msg["path"]))
				return;
			else
			{
				//send out a modulename_ready to the requesting session to inform its already ready:
				out.event=module.getName((string)msg["path"])+"_Ready";
				DEB("module is already ready, sending a " << out.event);
				out.dst=msg.src;
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
	//since permissions are very imporatant, make sure the user didnt make a typo.
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
						if (!(group=messageMan->userMan.getGroup(msg["modifyGroup"])))
							error+="Cant find modifygroup " +(string)msg["modifyGroup"]+ " ";
						else
							event->setModifyGroup(group);

					if (msg.isSet("recvGroup"))
						if (!(group=messageMan->userMan.getGroup(msg["recvGroup"])))
							error="Cant find recvgroup " + (string)msg["recvGroup"]+ " ";
						else
							event->setRecvGroup(group);

					if (msg.isSet("sendGroup"))
						if (!(group=messageMan->userMan.getGroup(msg["sendGroup"])))
							error+="Cant find sendgroup " + (string)msg["sendGroup"]+ " ";
						else
							event->setSendGroup(group);

				}
			}
		}
	}

	if (error!="")
		msg.returnError(error);
} 

/** core_Login
 * Check username and password and starts new session.
 * Sends: module_SessionStart to new session 
 * Sends: module_SessionStarted to broadcast
 * Sends: module_Login to src
 */
SYNAPSE_REGISTER(core_Login)
{
	string error;
	Cmsg startmsg;
	Cmsg loginmsg;

	{
		lock_guard<mutex> lock(messageMan->threadMutex);
		CsessionPtr session=messageMan->userMan.getSession(msg.src);
		if (!session)
			error="Session not found";
		else
		{
			CuserPtr user(messageMan->userMan.getUser(msg["username"]));
			if (!user || !user->isPassword(msg["password"]))
				error="Login invalid";
			else
			{
				CsessionPtr newSession=CsessionPtr(new Csession(user,session->module));
				int sessionId=messageMan->userMan.addSession(newSession);
				if (sessionId==SESSION_DISABLED)
					error="cant create new session";
				else
				{
					//set max threads?
					if (msg["maxThreads"] > 0)
						newSession->maxThreads=msg["maxThreads"];

					//send startmessage to the new session:
					startmsg.event="module_SessionStart";
					startmsg.dst=sessionId;
					startmsg["username"]=msg["username"];

					//send login message to the session that was requesting the login
					loginmsg.event="module_Login";
					loginmsg.dst=msg.src;
					loginmsg["username"]=msg["username"];
				}
			}
		}
	}

	if (error!="")
		msg.returnError(error);
	else
	{
		loginmsg.send();
		startmsg.send();
		//also broadcast module_SessionStarted, so other modules know that a session is started
		startmsg.event="module_SessionStarted";
		startmsg["session"]=startmsg.dst;
		startmsg.dst=0;
		startmsg.send();
	}
}

/** core_NewSession
 * Starts new session with the src user as owner
 * Sends: module_SessionStart to new session 
 * Sends: module_SessionStarted to broadcast
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
				//set max threads?
				if (msg["maxThreads"] > 0)
					newSession->maxThreads=msg["maxThreads"];

				//send startmessage to the new session:
				startmsg.event="module_SessionStart";
				startmsg.dst=sessionId;
				if (msg.isSet("pars"))
				{
					startmsg["pars"]=msg["pars"];
				}
				startmsg["username"]=session->user->getName();
			}
		}
	}

	if (error!="")
		msg.returnError(error);
	else
	{
		startmsg.send();
		//also broadcast module_SessionStarted, so other modules know that a session is started
		startmsg.event="module_SessionStarted";
		startmsg["session"]=startmsg.dst;
		startmsg.dst=0;
		startmsg.send();
	}
}


SYNAPSE_REGISTER(core_Logout)
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

		endmsg.event="module_SessionEnded";
		endmsg["session"]=endmsg.dst;
		endmsg.dst=0;
		endmsg.send();
	
		//now actually delete the session
		{
			lock_guard<mutex> lock(messageMan->threadMutex);
			if (!messageMan->userMan.delSession(msg.src))
				ERROR("cant delete session");
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
			session->module->ready=true;
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

SYNAPSE_REGISTER(core_Shutdown)
{
	{
		lock_guard<mutex> lock(messageMan->threadMutex);
		messageMan->doShutdown((int)msg["exit"]);
	}
}

SYNAPSE_REGISTER(core_ChangeLogging)
{
	{
		lock_guard<mutex> lock(messageMan->threadMutex);
		messageMan->logSends=msg["logSends"];
		messageMan->logReceives=msg["logReceives"];
	}
}

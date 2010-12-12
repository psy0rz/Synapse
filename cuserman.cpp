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













#include "cuserman.h"
#include <boost/shared_ptr.hpp>
#include <iostream>

#include "cmodule.h"
#include "clog.h"
#include "cconfig.h"

namespace synapse
{

using namespace std;
CuserMan::CuserMan()
{
	statMaxSessions=0;
	sessionCounter=0;
	sessionMaxPerUser=1000;
	shutdown=false;

	Cconfig config;
	config.load("etc/synapse/userman.conf");

	//create groups
	for(CvarList::iterator groupI=config["groups"].list().begin(); groupI!=config["groups"].list().end(); groupI++)
	{
		addGroup(CgroupPtr(new Cgroup(*groupI)));
	}

	//create users
	for(Cvar::iterator userI=config["users"].begin(); userI!=config["users"].end(); userI++)
	{
		//create user
		CuserPtr user;
		user=CuserPtr(new Cuser(userI->first, userI->second["passwd"].str()));

		//add member groups
		for(CvarList::iterator groupI=userI->second["groups"].list().begin(); groupI!=userI->second["groups"].list().end(); groupI++)
		{
			user->addMemberGroup(getGroup(*groupI));
		}

		addUser(user);
	}

}


CuserMan::~CuserMan()
{
}




/*!
    \fn CuserMan::getUser(string username)
 */
CuserPtr CuserMan::getUser(const string & userName)
{
	list<CuserPtr>::iterator userI;
	for (userI=users.begin(); userI!=users.end(); userI++)
	{
		if ((**userI).getName()==userName)
		{
			return(*userI);
		}
	}
	throw(runtime_error("User not found"));
	return (CuserPtr());
}





/*!
    \fn CuserMan::addUser(CuserPtr user)
 */
bool CuserMan::addUser(const CuserPtr & user)
{
	if (user)
	{
		users.push_back(user);
		return true;
	}
	return false;
}


/*!
    \fn CuserMan::addGroup(CgroupPtr group)
 */
bool CuserMan::addGroup(const CgroupPtr &group)
{
	if (group)
	{
		groups.push_back(group);
		return true;
	}
	return false;
}


/*!
    \fn CuserMan::getGroup(string groupName)
 */
CgroupPtr CuserMan::getGroup(const string &groupName)
{
	list<CgroupPtr>::iterator groupI;
	for (groupI=groups.begin(); groupI!=groups.end(); groupI++)
	{
		if ((**groupI).getName()==groupName)
		{
			return(*groupI);
		}
	}
	throw(runtime_error("Group not found"));
}


/*!
    \fn CuserMan::addSession(CsessionPtr session)
 */
int CuserMan::addSession( CsessionPtr session)
{
	if (shutdown)
	{
		ERROR("Shutting down, cant add new session.");
		return (SESSION_DISABLED);
	}

	//too much for this user already?
	int userSessions=0;
	int activeSessions=0;
	for (int sessionId=1; sessionId<MAX_SESSIONS; sessionId++)
	{
		CsessionPtr chkSession;
		chkSession=getSession(sessionId);
		if (chkSession)
		{
			activeSessions++;
			if (chkSession->user==session->user)
			{
				userSessions++;
				if (userSessions>=sessionMaxPerUser)
				{
					ERROR("User " << session->user->getName() << " has reached max sessions of " << sessionMaxPerUser);
					return (SESSION_DISABLED);
				}
			}
		}
	}

	//find free session ID. Start at the counter position, to prevent that we use the same numbers 
	//all the time. (which will be confusing)
	int startId=sessionCounter;
	do 
	{
		sessionCounter++;
		if (sessionCounter>=MAX_SESSIONS)
			sessionCounter=1;

		if (!sessions[sessionCounter])
		{
			//its free, store it here and return the ID.
			sessions[sessionCounter]=session;
			session->id=sessionCounter;
			DEB("added session " << sessionCounter << " with user " << session->user->getName());

			//keep stats
			activeSessions++;
			if (activeSessions>statMaxSessions)
				statMaxSessions=activeSessions;

			return (sessionCounter);
		}
	}
	while(startId!=sessionCounter);
	ERROR("Out of sessions, max=" << MAX_SESSIONS);
	return (SESSION_DISABLED);
}


/*!
    \fn CuserMan::getSession(int sessionId)
 */
CsessionPtr CuserMan::getSession(const int &sessionId)
{
	if (sessionId>=0 && sessionId<MAX_SESSIONS && sessions[sessionId]  )
	{
		if (sessions[sessionId]->isEnabled())
		{
			return (sessions[sessionId]);
		}
	}
	return (CsessionPtr());
}


/*!
    \fn CuserMan::delSession(int id)
 */
bool CuserMan::delSession(const int sessionId)
{
	if (sessionId>=0 && sessionId<MAX_SESSIONS && sessions[sessionId] )
	{
		//this is the default session of some module?
		if (sessions[sessionId]->module->defaultSessionId==sessionId)
		{
			//remove this session as default session from this module
			sessions[sessionId]->module->defaultSessionId=SESSION_DISABLED;

		}
		//reset shared ptr.
		//as soon as nobody uses the session object anymore it will be destroyed.
//NIET, is onhandig in de praktijk, ook bij shutdown:		sessions[sessionId]->id=SESSION_DISABLED;
		sessions[sessionId].reset();
		return (true);
	}
	else
	{
		return (false);
	}
}

list<int> CuserMan::delCookieSessions(int cookie, CmodulePtr module)
{
	list<int> deletedIds;

	if (cookie!=0)
	{
		for (int sessionId=1; sessionId<MAX_SESSIONS; sessionId++)
		{
			if (sessions[sessionId])
			{
				if (sessions[sessionId]->cookie==cookie && sessions[sessionId]->module==module)
				{
					delSession(sessionId);
					deletedIds.push_back(sessionId);
				}
			}
		}
	}
	return (deletedIds);
}


void CuserMan::getStatus(Cvar & var)
{
	int activeSessions=0;
	for (int sessionId=1; sessionId<MAX_SESSIONS; sessionId++)
	{
		CsessionPtr chkSession;
		chkSession=getSession(sessionId);
		if (chkSession)
		{
			activeSessions++;
			Cvar s;
			s["id"]=sessionId;
			s["user"]=chkSession->user->getName();
			s["module"]=chkSession->module->name;
			s["desc"]=chkSession->description;
			s["statSends"]=chkSession->statSends;
			s["statCalls"]=chkSession->statCalls;
			var["sessions"].list().push_back(s);
		}
	}

	var["statMaxSessions"]=statMaxSessions;
	var["activeSessions"]=activeSessions;
}

void CuserMan::doShutdown()
{
	shutdown=true;
}



string CuserMan::login(const int & sessionId, const string & userName, const string & password)
{
	CsessionPtr session=getSession(sessionId);
	if (!session)
		return (string("Session not found"));
	else
	{
		CuserPtr user(getUser(userName));
		if (!user || !user->isPassword(password))
			return (string("Login invalid"));
		else
		{
			//change user
			session->user=user;
			return (string());
		}
	}
}

}

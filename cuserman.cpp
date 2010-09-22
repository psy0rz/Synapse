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


//
// C++ Implementation: cuserman
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "cuserman.h"
#include <boost/shared_ptr.hpp>
#include <iostream>

#include "cmodule.h"
#include "clog.h"

namespace synapse
{

using namespace std;
CuserMan::CuserMan()
{
	sessionCounter=0;
	sessionMaxPerUser=1000;
	shutdown=false;

	addGroup(CgroupPtr(new Cgroup("core")));
	addGroup(CgroupPtr(new Cgroup("modules")));
	addGroup(CgroupPtr(new Cgroup("users")));
	addGroup(CgroupPtr(new Cgroup("everyone")));
	addGroup(CgroupPtr(new Cgroup("anonymous")));

	//default core user
	CuserPtr user;
	user=CuserPtr(new Cuser("core",""));
	user->addMemberGroup(getGroup("core"));
	user->addMemberGroup(getGroup("modules"));
	user->addMemberGroup(getGroup("everyone"));
	addUser(user);

	//default module user
	user=CuserPtr(new Cuser("module",""));
	user->addMemberGroup(getGroup("modules"));
	user->addMemberGroup(getGroup("everyone"));
	user->addMemberGroup(getGroup("anonymous"));
	addUser(user);

	//anonymous is only member of anonymous and probably only can send a core_Login.
	user=CuserPtr(new Cuser("anonymous","anonymous"));
	user->addMemberGroup(getGroup("anonymous"));
	addUser(user);

	//testusers
	user=CuserPtr(new Cuser("psy","as"));
	user->addMemberGroup(getGroup("users"));
	user->addMemberGroup(getGroup("everyone"));
	user->addMemberGroup(getGroup("anonymous"));
	addUser(user);

	//this admin receives EVERYTHING, including coreshizzle
	user=CuserPtr(new Cuser("admin","bs"));
	user->addMemberGroup(getGroup("users"));
	user->addMemberGroup(getGroup("core"));
	user->addMemberGroup(getGroup("modules"));
	user->addMemberGroup(getGroup("everyone"));
	user->addMemberGroup(getGroup("anonymous"));
	addUser(user);  

// 	for (int i=0; i<MAX_SESSIONS; i++)
// 		sessions[i]=CsessionPtr();

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
	ERROR("User " << userName << " not found!");
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
	return (CgroupPtr());
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
	for (int sessionId=1; sessionId<MAX_SESSIONS; sessionId++)
	{
		CsessionPtr chkSession;
		chkSession=getSession(sessionId);
		if (chkSession && chkSession->user==session->user)
		{
			userSessions++;
			if (userSessions>=sessionMaxPerUser)
			{
				ERROR("User " << session->user->getName() << " has reached max sessions of " << sessionMaxPerUser);
				return (SESSION_DISABLED);
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


string CuserMan::getStatusStr()
{
	stringstream s;
	for (int sessionId=0; sessionId<MAX_SESSIONS; sessionId++)
	{
		if (sessions[sessionId])
		{
			s << "session " << sessionId << " = " << sessions[sessionId]->user->getName() << "@" << sessions[sessionId]->module->name << ": " << sessions[sessionId]->description << "\n";
		}
	}
	return (s.str());
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

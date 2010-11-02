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













#ifndef CUSERMAN_H
#define CUSERMAN_H

#include "cuser.h"
#include "cgroup.h"
#include "csession.h"

#include <boost/shared_ptr.hpp>
#include <list>

namespace synapse
{

using namespace std;
using namespace boost;

#define MAX_SESSIONS 1000

//a session id consists of 2 x 16-bits
// -the higer 16 bits define the segment id, to identify the synapse instance.
// -the lower 16 bits define the local session id
#define SESSION_GET_SEGMENT(id) id >> 16;
#define SESSION_GET_LOCAL(id) id & 0xffff;
#define SESSION_GET_ID(segment,local)  (( segment << 16 ) | local )

/**
	@author 
*/
class CuserMan{
public:
	CuserMan();
	
	~CuserMan();


	CuserPtr getUser(const string & userName);
	bool addUser(const CuserPtr & user);
	CgroupPtr getGroup(const string & groupName);
	bool addGroup(const CgroupPtr &group);
	int addSession( CsessionPtr session);
	CsessionPtr getSession(const int & sessionId);
	bool delSession(const int id);
	list<int> delCookieSessions(int cookie, CmodulePtr module);
    string getStatusStr();
	void doShutdown();
	string login(const int & sessionId, const string & userName, const string & password);

private:
	bool shutdown;
	list<CuserPtr> users;
	list<CgroupPtr> groups;
	//performance: we use an oldskool array, so session lookups are quick
	CsessionPtr sessions[MAX_SESSIONS+1];

	int sessionMaxPerUser;
	int sessionCounter;
	int sessionSegment; //every synapse instance should have a uniq segment

};
}

#endif

//
// C++ Interface: cuserman
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CUSERMAN_H
#define CUSERMAN_H

#include "cuser.h"
#include "cgroup.h"
#include "csession.h"

#include <boost/shared_ptr.hpp>
#include <list>
using namespace std;
using namespace boost;

#define MAX_SESSIONS 1000

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
    void print();
	void doShutdown();
	string login(const int & sessionId, const string & userName, const string & password);

private:
	bool shutdown;
	list<CuserPtr> users;
	list<CgroupPtr> groups;
	//performance: we use an oldskool array, so session lookups are quick
	int sessionCounter;
	CsessionPtr sessions[MAX_SESSIONS+1];
	int sessionMaxPerUser;
	
};

#endif

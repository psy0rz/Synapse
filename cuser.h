//
// C++ Interface: cuser
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CUSER_H
#define CUSER_H
#include "cgroup.h"
#include <string>
#include <list>
#include <boost/shared_ptr.hpp>

namespace synapse
{
using namespace boost;
using namespace std;

/**
	@author 
*/
class Cuser{
public:
    Cuser(const string &name, const string &password);

    ~Cuser();
	bool addMemberGroup(const CgroupPtr & group);
    bool isMemberGroup(const CgroupPtr & group);
    string getName();
    bool isPassword(const string & password);
    void print();

private:
	string name;
	string password;
	list<CgroupPtr > memberGroups;
	 	
};

typedef shared_ptr<Cuser> CuserPtr;

}

#endif

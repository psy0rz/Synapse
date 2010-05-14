//
// C++ Interface: cevent
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CEVENT_H
#define CEVENT_H
#include <string>
#include "cgroup.h"
#include "cuser.h"

#include <boost/shared_ptr.hpp>
using namespace boost;
using namespace std;



/**
	@author 
*/
class Cevent{
public:
	Cevent(const CgroupPtr & modifyGroup, const CgroupPtr & sendGroup, const CgroupPtr & recvGroup );

	~Cevent();
  	bool isSendAllowed(const CuserPtr & user);
	bool isRecvAllowed(const CuserPtr & user);
	bool isSendAllowed(const CgroupPtr & group);
	bool isRecvAllowed(const CgroupPtr & group);
	bool isModifyAllowed(const CuserPtr & user);

	void setRecvGroup(const CgroupPtr & group);
	void setSendGroup(const CgroupPtr & group);
	void setModifyGroup(const CgroupPtr & group);
    void print();


private:	

	CgroupPtr sendGroup;
	CgroupPtr recvGroup;
	CgroupPtr modifyGroup;

};

typedef shared_ptr<Cevent> CeventPtr;

#endif

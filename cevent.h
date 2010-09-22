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













#ifndef CEVENT_H
#define CEVENT_H
#include <string>
#include "cgroup.h"
#include "cuser.h"

#include <boost/shared_ptr.hpp>

namespace synapse
{

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


	CgroupPtr getRecvGroup();
	CgroupPtr getSendGroup();
	CgroupPtr getModifyGroup();

private:	

	CgroupPtr sendGroup;
	CgroupPtr recvGroup;
	CgroupPtr modifyGroup;

};

typedef shared_ptr<Cevent> CeventPtr;

}

#endif

/*
 * csession.cpp
 *
 *  Created on: Jul 14, 2010
 *      Author: psy
 */
#include "cgroup.h"
namespace asterisk
{

	//sessions: every synapse session has a corresponding session object here
	Csession::Csession(int id)
	{
		this->id=id;
	}


	void Csession::setGroupPtr(CgroupPtr groupPtr)
	{
		this->groupPtr=groupPtr;
	}

	CgroupPtr Csession::getGroupPtr()
	{
		return (groupPtr);
	}

	string Csession::getStatus(string prefix)
	{
		stringstream s;
		s << prefix <<  "Session " << id << ":\n" << groupPtr->getStatus(prefix+" ");
		return(s.str());
	}

}

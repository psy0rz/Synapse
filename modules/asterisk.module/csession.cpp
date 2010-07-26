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
		if (groupPtr)
			s << prefix <<  "Session " << id << ":\n" << groupPtr->getStatus(prefix+" ");
		else
			s << prefix <<  "Session " << id << "\n" << prefix << "(not logged in)\n";
		return(s.str());
	}

}

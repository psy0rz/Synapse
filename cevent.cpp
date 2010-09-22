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













#include "clog.h"
#include "cevent.h"
#include <iostream>
namespace synapse
{


Cevent::Cevent(const CgroupPtr & modifyGroup, const CgroupPtr & sendGroup, const CgroupPtr & recvGroup )
{
	this->modifyGroup=modifyGroup;
	this->sendGroup=sendGroup;
	this->recvGroup=recvGroup;
}


Cevent::~Cevent()
{
}





bool Cevent::isSendAllowed(const CuserPtr & user)
{
	return (
		user &&
		(
			(sendGroup && user->isMemberGroup(sendGroup)) 
		)
	);
}

bool Cevent::isSendAllowed(const CgroupPtr & group)
{
	return (group && sendGroup && group==sendGroup);
}

bool Cevent::isRecvAllowed(const CuserPtr & user)
{

	return (
		user &&
		(
			(recvGroup && user->isMemberGroup(recvGroup)) 
		)
	);
}

bool Cevent::isRecvAllowed(const CgroupPtr & group)
{
	return (group && recvGroup && group==recvGroup);
}


bool Cevent::isModifyAllowed(const CuserPtr & user)
{
	return (
		user &&
		(
			(modifyGroup && user->isMemberGroup(modifyGroup)) 
		)
	);
}


void Cevent::setSendGroup(const CgroupPtr & group)
{
    sendGroup=group;
}


void Cevent::setRecvGroup(const CgroupPtr & group)
{
    recvGroup=group;
}

void Cevent::setModifyGroup(const CgroupPtr & group)
{
    modifyGroup=group;
}

CgroupPtr Cevent::getSendGroup()
{
	return(sendGroup);
}


CgroupPtr Cevent::getRecvGroup()
{
	return(recvGroup);
}

CgroupPtr Cevent::getModifyGroup()
{
	return(modifyGroup);
}



/*!
    \fn Cevent::print()
 */
void Cevent::print()
{
	DEB("recvGroup  =" << recvGroup->getName());
	DEB("sendGroup  =" << sendGroup->getName());
	DEB("modifyGroup=" << modifyGroup->getName());
}

}

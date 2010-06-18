//
// C++ Implementation: cevent
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "clog.h"
#include "cevent.h"
#include <iostream>


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

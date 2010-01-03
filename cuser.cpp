//
// C++ Implementation: cuser
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "cuser.h"
#include "clog.h"

Cuser::Cuser(const string &name, const string &password="")
{
	this->name=name;
	this->password=password;
}


Cuser::~Cuser()
{
}




/*!
    \fn Cuser::addMemberGroup(CgroupPtr group)
 */
bool Cuser::addMemberGroup(const CgroupPtr & group)
{
	if (group)
	{
		memberGroups.push_back(group);
		return true;
	}
	return false;
}


/*!
    \fn Cuser::isMemberGroup(CgroupPtr group)
 */
bool Cuser::isMemberGroup(const CgroupPtr & group)
{
	if (!group)
		return false;

	list<CgroupPtr>::iterator groupI;
	for (groupI=memberGroups.begin(); groupI!=memberGroups.end(); groupI++)
	{
		if ((*groupI)==(group))
		{
			return true;
		}
	}
	return false;
}


/*!
    \fn Cuser::getName()
 */
string Cuser::getName()
{
	return (name);
}


/*!
    \fn Cuser::isPassword(string password)
 */
bool Cuser::isPassword(const string & password)
{
	if (password!="" && this->password!="")
	    return (this->password==password);
	else
		return false;
}


/*!
    \fn Cuser::print()
 */
void Cuser::print()
{
	list<CgroupPtr>::iterator groupI;
	DEB(name << " with password '" << password << "' is member of:");
	for (groupI=memberGroups.begin(); groupI!=memberGroups.end(); groupI++)
	{
		DEB( (*groupI)->getName());
	}

}

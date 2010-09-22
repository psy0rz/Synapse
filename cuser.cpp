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
namespace synapse
{

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
	//blank passwords not allowed, they are used for module and core users.

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
}

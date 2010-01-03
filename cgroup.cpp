//
// C++ Implementation: cgroup
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "cgroup.h"

Cgroup::Cgroup(const string & name)
{
	this->name=name;
}


Cgroup::~Cgroup()
{
}




/*!
    \fn Cgroup::getName()
 */
string Cgroup::getName()
{
	return (name);
}

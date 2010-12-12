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













#include "csession.h"
#include "cuser.h"
#include "cmodule.h"
#include <iostream>

namespace synapse
{

Csession::Csession(const CuserPtr &user, const CmodulePtr &module, int cookie)
{
	this->user=user;
	this->module=module;
	this->cookie=cookie;
	id=SESSION_DISABLED;
	maxThreads=1;
	currentThreads=0;
	statCalls=0;
	statSends=0;
}


Csession::~Csession()
{
}






/*!
    \fn Csession::isEnabled()
 */
bool Csession::isEnabled()
{
    return (id!=SESSION_DISABLED);
}


/*!
    \fn Csession::startThread()
 */
bool Csession::startThread()
{
	if (currentThreads<maxThreads)
	{
		if (module->startThread())
		{
			currentThreads++;
			statCalls++;
			return true;
		}
	}
	return false;
}


/*!
    \fn Csession::endThread()
 */
void Csession::endThread()
{
	module->endThread();
	currentThreads--;
	assert(currentThreads>=0);
}

}

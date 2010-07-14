//
// C++ Implementation: csession
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
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

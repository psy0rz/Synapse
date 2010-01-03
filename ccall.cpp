//
// C++ Implementation: ccall
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "ccall.h"

Ccall::~Ccall()
{
}

Ccall::Ccall()
{
	started=false;
}

Ccall::Ccall(const CmsgPtr & msg,const CsessionPtr & dst, FsoHandler soHandler)
{
	started=false;
	this->msg=msg;
	this->dst=dst;
	this->soHandler=soHandler;
}



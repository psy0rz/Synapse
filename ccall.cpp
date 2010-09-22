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
namespace synapse
{
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


}

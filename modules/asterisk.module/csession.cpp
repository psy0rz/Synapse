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


/*
 * csession.cpp
 *
 *  Created on: Jul 14, 2010

 */


#include "csession.h"
#include "cdevice.h"
#include <sstream>

namespace asterisk
{

	//sessions: every synapse session has a corresponding session object here
	Csession::Csession(int id)
	{
		this->id=id;
		admin=false;
		authorized=false;

	}

	void Csession::authorize(CserverPtr serverPtr, CdevicePtr devicePtr)
	{
		this->devicePtr=devicePtr;
		this->serverPtr=serverPtr;
		this->authorized=true;
	}

	void Csession::deauthorize()
	{
		this->devicePtr=CdevicePtr();
		this->serverPtr=CserverPtr();
		this->authorized=false;
	}

	bool Csession::isAuthorized()
	{
		return(authorized);
	}

	bool Csession::isAdmin()
	{
		return(admin);
	}

	int Csession::getId()
	{
		return(id);
	}

	CdevicePtr Csession::getDevicePtr()
	{
		return (devicePtr);
	}

	CserverPtr Csession::getServerPtr()
	{
		return (serverPtr);
	}

	string Csession::getStatus(string prefix)
	{
		stringstream s;
		if (devicePtr)
			s << prefix <<  "Session " << id << ":\n" << devicePtr->getStatus(prefix+" ");
		else
			s << prefix <<  "Session " << id << "\n" << prefix << "(not logged in)\n";
		return(s.str());
	}





}

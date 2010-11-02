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













#include "cmsg.h"
#include "clog.h"
#include <stdarg.h>




Cmsg::Cmsg()
{
	dst=DST_CORE;
	src=0;
}


Cmsg::~Cmsg()
{
}


void Cmsg::toJson(string & jsonStr)
{
	//convert the message to a json-message
	Array spiritMsg;
	spiritMsg.push_back(src);	
	spiritMsg.push_back(dst);
	spiritMsg.push_back(event);
	
	//convert the parameters, if any
	if (!isEmpty())
	{
		Value spiritValue;
		Cvar::toJsonSpirit(spiritValue);
	
		spiritMsg.push_back(spiritValue);
	}

	jsonStr=write(spiritMsg);
}


void Cmsg::fromJson(string & jsonStr)
{

	Value spiritMsg;
	readJsonSpirit(jsonStr, spiritMsg);

	if (spiritMsg.get_array().size()>2)
	{
		src=spiritMsg.get_array()[0].get_int();
		dst=spiritMsg.get_array()[1].get_int();
		event=spiritMsg.get_array()[2].get_str();
	}
	if (spiritMsg.get_array().size()>3)
		fromJsonSpirit(spiritMsg.get_array()[3]);
}



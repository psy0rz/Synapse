//
// C++ Implementation: cmsg
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "cmsg.h"
#include "clog.h"
#include <stdarg.h>




Cmsg::Cmsg()
{
	dst=0;
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
	//TODO:how safe is it actually to let json_spirit parse untrusted input? (regarding DoS, buffer overflows, etc)
	json_spirit::read(jsonStr, spiritMsg);

	if (spiritMsg.get_array().size()>2)
	{
		src=spiritMsg.get_array()[0].get_int();
		dst=spiritMsg.get_array()[1].get_int();
		event=spiritMsg.get_array()[2].get_str();
	}
	if (spiritMsg.get_array().size()>3)
		fromJsonSpirit(spiritMsg.get_array()[3]);
}



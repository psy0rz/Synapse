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













#ifndef CCALL_H
#define CCALL_H
#include "csession.h"
#include "cmsg.h"
#include <boost/thread/thread.hpp>

namespace synapse
{
typedef boost::shared_ptr<thread> CthreadPtr; 

/**
	@author
*/
class Ccall{
public:
    Ccall(const CmsgPtr & msg,const CsessionPtr & dst, FsoHandler soHandler);
    Ccall();

    ~Ccall();
	CsessionPtr dst;
	CmsgPtr msg;
	FsoHandler soHandler;
	bool started;
	CthreadPtr threadPtr;

};
}
#endif

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













#ifndef CSESSION_H
#define CSESSION_H

#include <boost/shared_ptr.hpp>
    
namespace synapse
{
	using namespace boost;
	using namespace std;


	typedef shared_ptr<class Csession> CsessionPtr;
}

#include "cmodule.h"
#include "cuser.h"

namespace synapse
{
using namespace boost;
using namespace std;
/*
	disabled sessions have id SESSION_DISABLED (-1)
	broadcasts go to sessions id 0
	core always has session id 1
	rest of the world has 2 and higher.
*/
#define SESSION_DISABLED -1



/**
	@author 
*/
class Csession{
public:
    Csession(const CuserPtr &user, const CmodulePtr &module, int cookie=0);

    ~Csession();
    bool isEnabled();
    void endThread();
    bool startThread();

	CuserPtr user;
	CmodulePtr module;
	int	id;
	int cookie; //arbitrary value, set by module for internal use
	int maxThreads;
	int currentThreads;
	int statCalls;
	int statSends;
	string description;


private:
	
};

}
#endif

//
// C++ Interface: csession
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
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
	int cookie; //random value, set by module for internal use
	int maxThreads;
	int currentThreads;


private:
	
};

}
#endif

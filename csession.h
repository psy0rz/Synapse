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
using namespace boost;
using namespace std;

typedef shared_ptr<class Csession> CsessionPtr;

#include "cmodule.h"
#include "cuser.h"


#define SESSION_DISABLED -1

/**
	@author 
*/
class Csession{
public:
    Csession(const CuserPtr &user, const CmodulePtr &module);

    ~Csession();
    bool isEnabled();
    void endThread();
    bool startThread();
	CuserPtr user;
	CmodulePtr module;
	int	id;
	int maxThreads;
	int currentThreads;


private:
	
};


#endif

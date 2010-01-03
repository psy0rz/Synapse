//
// C++ Interface: ccall
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CCALL_H
#define CCALL_H
#include "csession.h"
#include "cmsg.h"
#include <boost/thread/thread.hpp>

typedef shared_ptr<thread> CthreadPtr; 

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

#endif

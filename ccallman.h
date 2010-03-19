//
// C++ Interface: ccallman
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CCALLMAN_H
#define CCALLMAN_H
#include "cmsg.h"
#include "csession.h"
#include "ccall.h"

/**
	@author 
*/

typedef list<Ccall> CcallList;

class CcallMan{
public:
	CcallMan();
	
	~CcallMan();
	void addCall(const CmsgPtr & msg, const CsessionPtr & dst, FsoHandler soHandler);
	CcallList::iterator startCall(const CthreadPtr & threadPtr);
	void endCall(CcallList::iterator callI);
	bool interruptCall(string event, int src, int dst);
	bool interruptAll();
	string getStatusStr(bool queue, bool verbose);

	int statsTotal;

	CcallList callList;
	
};

#endif

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













#include "ccallman.h"
#include "clog.h"
using namespace boost;
namespace synapse
{

CcallMan::CcallMan()
{
	statCallsTotal=0;
	statCallsQueuedMax=0;
	statCallsQueued=0;
}


CcallMan::~CcallMan()
{
}




/*!
    \fn CcallMan::addCall(CmsgPtr msg, CsessionPtr dst)
 */
void CcallMan::addCall(const CmsgPtr & msg, const CsessionPtr & dst, FsoHandler soHandler)
{
	callList.push_back(Ccall(msg,dst,soHandler));
	statCallsTotal++;
	statCallsQueued++;
	if (statCallsQueued>statCallsQueuedMax)
		statCallsQueuedMax=statCallsQueued;
//	DEB( "(" << callList.size() << ")" << " " << msg->event << " FROM " << msg->src << " TO " << msg->dst << " CALLDST " << dst->id);
}






/*!
    \fn CcallMan::popCall()
 */
CcallList::iterator CcallMan::startCall(const CthreadPtr & threadPtr)
{
	//find a call thats ready to go and return it
	//TODO:optimize 
	for (CcallList::iterator callI=callList.begin(); callI!=callList.end(); callI++)
	{
		if (!callI->started && callI->dst->startThread())
		{
			callI->threadPtr=threadPtr;
			callI->started=1;
			statCallsQueued--;
//			DEB( callI->msg->event << " FROM " << callI->msg->src << " TO " << callI->msg->dst << " CALLDST " << callI->dst->id);
		
			return callI;
		}
	}
	return CcallList::iterator();
}

void CcallMan::endCall(CcallList::iterator callI)
{
	callI->dst->endThread();
//	DEB( callI->msg->event << " FROM " << callI->msg->src << " TO " << callI->msg->dst << " CALLDST " << callI->dst->id);
	
	callList.erase(callI);
//	DEB("calls left: " << callList.size());
}


void CcallMan::getStatus(Cvar & var)
{

	for (CcallList::iterator callI=callList.begin(); callI!=callList.end(); callI++)
	{
		Cvar c;
		c["running"]=callI->started;
		c["event"]=callI->msg->event;
		c["src"]=callI->msg->src;
		c["dst"]=callI->dst->id;
		c["dstUserName"]=callI->dst->user->getName();
		c["dstModule"]=callI->dst->module->name;
		c["data"]=*callI->msg;
		var["queue"].list().push_back(c);
	}

	var["statCallsQueued"]=statCallsQueued;
	var["statCallsQueuedMax"]=statCallsQueuedMax;
	var["statCallsTotal"]=statCallsTotal;

}

bool CcallMan::interruptCall(string event, int src, int dst)
{
	for (CcallList::iterator callI=callList.begin(); callI!=callList.end(); callI++)
	{
		if (callI->msg->event==event)
		{
			if (callI->msg->src==src)
			{
				if (dst==0 || dst==callI->dst->id)
				{	
					//send interrupt
					if (callI->started && callI->threadPtr)
					{
						DEB("Interrupting call: " << event << " FROM " << src << " TO " << dst);
						callI->threadPtr->interrupt();
						return (true);
					}
					//not started yet, we can just delete it from the queue
					{
						DEB("Cancelling call: " << event << " FROM " << src << " TO " << dst);
						callList.erase(callI);
						statCallsQueued--;
						return (true);

					}
				}
			}
		}
	}
	return false;
}

//interrupt all running calls (used for shutdown)
bool CcallMan::interruptAll()
{
	for (CcallList::iterator callI=callList.begin(); callI!=callList.end(); callI++)
	{
		//send interrupt
		if (callI->started && callI->threadPtr)
		{
			DEB("Interrupting call: " << callI->msg->event << " FROM " << callI->msg->src << " TO " <<
			callI->dst->id << ":" << callI->dst->user->getName() << "@" << callI->dst->module->name 
			<< callI->msg->getPrint("  |"));

			callI->threadPtr->interrupt();
		}
	}
	return false;
}
}

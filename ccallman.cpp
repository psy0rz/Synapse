//
// C++ Implementation: ccallman
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "ccallman.h"
#include "clog.h"
using namespace boost;
namespace synapse
{

CcallMan::CcallMan()
{
	statsTotal=0;
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
	statsTotal++;
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


/*!
    \fn CcallMan::print()
 */
string CcallMan::getStatusStr(bool queue, bool verbose)
{
	stringstream status;
	int running=0;

	for (CcallList::iterator callI=callList.begin(); callI!=callList.end(); callI++)
	{
		if (queue)
		{
			status << " |";
			if (callI->started)
				status << "RUNNING";
			else if (!verbose)
				continue;
			else
				status << "QUEUED ";
	
			status << " " << callI->msg->event << 
					" FROM " << callI->msg->src << 
					" TO " << callI->dst->id << ":" << callI->dst->user->getName() << 
					"@" << callI->dst->module->name << callI->msg->getPrint("  |") <<
					"\n";
		}

		if (callI->started)
			running++;
	}

	status << statsTotal << " calls processed, " << running << "/" << callList.size() << " calls running.\n";

	return (status.str());
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

/*
 * cgroup.cpp
 *
 *  Created on: Jul 14, 2010
 *      Author: psy
 */

#include "cgroup.h"
#include <string.h>
//groups: most times a tennant is considered a group.
//After authenticating, a session points to a group.'
//All devices of a specific tennant also point to this group
//events are only sent to Csessions that are member of the same group as the corresponding device.
namespace asterisk
{
	Cgroup::Cgroup()
	{
	}

	void Cgroup::setId(string id)
	{
		this->id=id;
	}

	string Cgroup::getId()
	{
		return(id);
	}

	//sends msg after applying filtering.
	//message will only be sended or broadcasted to sessions that belong to this group.
	//some channels like locals and trunks will also be filtered, depending on session-specific preferences
	void Cgroup::send(CsessionMap & sessionMap, Cmsg & msg)
	{
		//broadcast?
		if (msg.dst==0)
		{
			//we cant simply broadcast it, we need to check group membership session by session
			for (CsessionMap::iterator I=sessionMap.begin(); I!=sessionMap.end(); I++)
			{
				if (I->second->isAdmin || I->second->getGroupPtr().get()==this)
				{
					msg.dst=I->first;
					try
					{
						msg.send();
					}
					catch(const std::exception& e)
					{
						WARNING("asterisk delivering broadcast to " << msg.dst << " failed: " << e.what());
					}

				}
			}
			//restore dst value:
			msg.dst=0;
		}
		else
		{
			CsessionMap::iterator I=sessionMap.find(msg.dst);
			if (I!=sessionMap.end())
			{
				if (I->second->isAdmin || I->second->getGroupPtr().get()==this)
				{
					try
					{
						msg.send();
					}
					catch(const std::exception& e)
					{
						WARNING("asterisk send to " << msg.dst << " failed: " << e.what());
					}
				}
			}
		}

	}

	string Cgroup::getStatus(string prefix)
	{
		return (
			prefix+"Group "+id
		);
	}



}

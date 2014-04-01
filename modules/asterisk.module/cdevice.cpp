#include "cgroup.h"
#include "cdevice.h"

namespace asterisk
{
	Cdevice::Cdevice()
	{
		

		changed=true;
		online=true;
		trunk=true; //assume true, to prevent showing trunks on asterisk-reconnect
	}

	//check if the device should be filtered from the endusers, according to various filtering options
	bool Cdevice::isFiltered()
	{
		//dont show trunks
		// if (trunk)
		// 	return (true);

		//dont show devices with empty caller ID's
		// if (callerIdName=="")
		// 	return (true);

		//dont show anything thats not sip (for now)
		if (id.substr(0,3)!="SIP")
			return (true);

		return(false);
	}

	CgroupPtr Cdevice::getGroupPtr()
	{
		return (groupPtr);
	}

	TauthCookie Cdevice::getAuthCookie()
	{
		string authKey=groupPtr->getId()+"."+id;
		//create new cookie?
		if (!stateDb["cookies"].isSet(authKey))
		{
			TauthCookie authCookie;
			mrand48_r(&randomBuffer, &authCookie);
			stateDb["cookies"][authKey]=authCookie;
			stateDb.changed();
		}
			
		return(stateDb["cookies"][authKey]);
	}


	void Cdevice::setId(string id)
	{
		if (id!=this->id)
		{
			this->id=id;
			if (callerId=="")
			{
				callerId=id;
			}
			changed=true;
		}
	}

	void Cdevice::setGroupPtr(CgroupPtr groupPtr)
	{
		this->groupPtr=groupPtr;
		changed=true;
	}

	string Cdevice::getId()
	{
		return (id);
	}

	void Cdevice::setCallerId(string callerId)
	{
		if (callerId!="" && callerId!=this->callerId)
		{
			this->callerId=callerId;
			changed=true;
		}
	}

	string Cdevice::getCallerId()
	{
		return(callerId);
	}

	void Cdevice::setCallerIdName(string callerIdName)
	{
		if (callerIdName!="" && callerIdName!=this->callerIdName)
		{
			this->callerIdName=callerIdName;
			changed=true;
		}
	}

	string Cdevice::getCallerIdName()
	{
		return(callerIdName);
	}


	void Cdevice::setTrunk(bool trunk)
	{
		this->trunk=trunk;
		changed=true;
	}

	void Cdevice::setOnline(bool online)
	{
		if (online!=this->online)
		{
			this->online=online;
			changed=true;
		}
	}

	void Cdevice::setExten(string exten)
	{
		this->exten=exten;
	}


	bool Cdevice::sendUpdate(int forceDst)
	{
		if (groupPtr==NULL)	
			return(false);

		if (isFiltered())
			return(false);

		if (groupPtr!=NULL)
		{
			Cmsg out;
			out.event="asterisk_updateDevice";
			out.dst=forceDst;
			out["id"]=id;
			out["callerId"]=callerId;
			out["callerIdName"]=callerIdName;
			out["online"]=online;
			out["trunk"]=trunk;
			out["exten"]=exten;
			out["groupId"]=groupPtr->getId();
			groupPtr->send(sessionMap,out);
		}

		return(true);
	}


	void Cdevice::sendChanges()
	{
		if (changed)
		{	
			if (sendUpdate())
				changed=false;
		}
	}


	void Cdevice::sendRefresh(int dst)
	{
		//refresh device
		sendUpdate(dst);
	}

	string Cdevice::getStatus(string prefix)
	{
		return (
			prefix+"Device "+id+":\n"+
			groupPtr->getStatus(prefix+" ")
		);
	}


	Cdevice::~Cdevice()
	{
		Cmsg out;
		out.event="asterisk_delDevice";
		out.dst=0; 
		out["id"]=id;

		if (groupPtr!=NULL)
			groupPtr->send(sessionMap,out);
	}
};

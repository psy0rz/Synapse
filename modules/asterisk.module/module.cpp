/** \file
Asterisk Control Module.

Uses the AMI module (and maybe others) to track and control asterisk.

\code
To capture test-data from a asterisk server:
  script -c telnet 1.2.3.4 5038 -t ami.txt 2> ami.timing

and login with:
 Action: Login
 ActionID: Login
 Events: on
 Secret: password
 UserName: username



To setup a fake server replaying this:

 tcpserver 0.0.0.0 5555 scriptreplay ami.timing ami.txt
 

\endcode



*/

#include "synapse.h"
#include <boost/regex.hpp>
#include <boost/foreach.hpp>



SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=1; //NOTE: this module is single threaded only!
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=1;
	out.send();

	out.clear();
	out.event="core_LoadModule";
 	out["path"]="modules/ami.module/libami.so";
 	out.send();

	//FIXME  clean/permissions

	out.clear();
	out.event="core_ChangeEvent";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"anonymous";



	out["event"]=		"asterisk_debugChannel"; 
	out.send();

	out["event"]=		"asterisk_updateChannel"; 
	out.send();

	out["event"]=		"asterisk_delChannel"; 
	out.send();

	out["event"]=		"asterisk_updateDevice"; 
	out.send();

	out["event"]=		"asterisk_delDevice"; 
	out.send();

	out["event"]=		"asterisk_refresh"; 
	out.send();

	out["event"]=		"asterisk_reset"; 
	out.send();

}




namespace ami
{

	string getDeviceIdFromChannel(string channel)
	{
		//a channel can be either:
		// technology/number-random
		//or
		// agent/number
		smatch what;
		if (!regex_search(
			channel,
			what, 
			boost::regex("^(.*)-([^-]*)$")
		))
		{
			//return whole string, appearently there is no random channel identifier added. (happens with agents)
			return (channel);
//			WARNING("Invalid channel: " << channel << ", using 'unknown' instead");
//	/		return (string("unknown"));
		}
		else
		{
			return (what[1]);
		}
	}

	typedef shared_ptr<class Cdevice> CdevicePtr;
	typedef map<string, CdevicePtr> CdeviceMap;
	typedef shared_ptr<class Cchannel> CchannelPtr;
	typedef map<string, CchannelPtr> CchannelMap;

	
	
	
	class Cdevice
	{
		private: 

		bool online;
		string callerId;
		string id;
		bool changed;
	
		public:

		Cdevice()
		{
			changed=true;
			id="";
			online=true;
		}

		void setId(string id)
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

		string getId()
		{
			return (id);
		}
	
		void setCallerId(string callerId)
		{
			if (callerId!="" && callerId!=this->callerId)
			{
				this->callerId=callerId;
				changed=true;
			}
		}

		void setOnline(bool online)
		{
			if (online!=this->online)
			{
				this->online=online;
				changed=true;
			}
		}


	

		void sendUpdate(int forceDst=0)
		{
			Cmsg out;
			out.event="asterisk_updateDevice";
			out.dst=forceDst;
			out["id"]=id;
			out["callerId"]=callerId;
			out["online"]=online;
			out.send();
		}


		void sendChanges()
		{
			if (changed)
			{	
				sendUpdate();

				changed=false;
			}
		}


		void sendRefresh(int dst)
		{
			//refresh device
			sendUpdate(dst);

		}

		~Cdevice()
		{
			Cmsg out;
			out.event="asterisk_delDevice";
			out.dst=0; 
			out["id"]=id;
			out.send();
		}

		
	};
	
	class Cchannel
	{
		private:

		string id;
		int changes;
		string state;
		CchannelPtr linkChannelPtr;
		string callerId;
		string callerIdName;
		string linkCallerId;
		string linkCallerIdName;
		string firstExtension;
		bool initiator;
		CdevicePtr devicePtr;

		int changesSent;		

		public:

		Cchannel()
		{
			changes=1;
			changesSent=0;
			initiator=true;
		}

		void sendDebug(Cmsg msg, int serverId)
		{
			msg.event="asterisk_debugChannel";
			msg["serverId"]=serverId;
			msg["id"]=id;
			msg.dst=0;
			msg.src=0;
			msg.send();
		}

		int getChanges()
		{
			return (changes);
		}


		void setFirstExtension(string firstExtension)
		{
			if (firstExtension!=this->firstExtension)
			{
				this->firstExtension=firstExtension;
				changes++;
			}

		}

		string getFirstExtension()
		{
			return(firstExtension);
		}

		void setInitiator(bool initiator)
		{
			if (initiator!=this->initiator)
			{
				this->initiator=initiator;
				changes++;
			}

		}

		void setDevice(CdevicePtr devicePtr)
		{
			if (devicePtr!=this->devicePtr)
			{
				this->devicePtr=devicePtr;
				changes++;
			}

		}

		void setId(string id)
		{
			if (id!=this->id)
			{
				this->id=id;
				changes++;
			}
		}

		void setLink(CchannelPtr channelPtr)
		{
			if (channelPtr!=this->linkChannelPtr)
			{
				this->linkChannelPtr=channelPtr;
				if (linkChannelPtr!=CchannelPtr())
				{
					linkChannelPtr->setLinkCallerId(callerId);
					linkChannelPtr->setLinkCallerIdName(callerIdName);
				}
				changes++;
			}
		}

		void delLink()
		{
			//unset our link
			setLink(CchannelPtr());
		}

		CchannelPtr getLink()
		{
			return(linkChannelPtr);
		}


		void setCallerId(string callerId)
		{
			if (callerId!=this->callerId)
			{
				this->callerId=callerId;
				if (linkChannelPtr!=CchannelPtr())
					linkChannelPtr->setLinkCallerId(callerId);
				changes++;
			}
		}

		void setCallerIdName(string callerIdName)
		{
			if (callerIdName!=this->callerIdName)
			{
				this->callerIdName=callerIdName;
				if (linkChannelPtr!=CchannelPtr())
					linkChannelPtr->setLinkCallerIdName(callerIdName);
				changes++;
			}
		}

		void setLinkCallerId(string callerId)
		{
			if (callerId!=this->linkCallerId)
			{
				this->linkCallerId=callerId;
				changes++;
			}
		}

		void setLinkCallerIdName(string callerIdName)
		{
			if (callerIdName!=this->linkCallerIdName)
			{
				this->linkCallerIdName=callerIdName;
				changes++;
			}
		}


		string getCallerId()
		{
			return (callerId);
		}

		string getCallerIdName()
		{
			return (callerIdName);
		}

		void setState(string state)
		{
	
			if (state!=this->state)
			{
				this->state=state;
				changes++;
			}
		}

		void sendChanges(bool recursing=false)
		{
			bool sendIt=false;

			// are there changes we didnt send yet?
			if (changes>changesSent)
			{
				sendIt=true;
				changesSent=changes;
			}	

			//are we linked?
			if (linkChannelPtr!=CchannelPtr())
			{
				//let the other channel check for changes as well
				//(prevent endless recursion)
				if (!recursing)
					linkChannelPtr->sendChanges(true);
			}

			if (sendIt)
				sendUpdate();
		}

		void sendUpdate(int forceDst=0)
		{
			Cmsg out;
			out.event="asterisk_updateChannel";
			out.dst=forceDst;
			out["id"]=id;
			out["state"]=state;
			out["initiator"]=initiator;
			out["callerId"]=callerId;
			out["callerIdName"]=callerIdName;

			if (devicePtr!=CdevicePtr())
			{
				out["deviceId"]=devicePtr->getId();
			}

			out["linkCallerId"]=linkCallerId;
			out["linkCallerIdName"]=linkCallerIdName;

			out["firstExtension"]=firstExtension;

			out.send();
		}

		void sendRefresh(int dst)
		{
			sendUpdate(dst);
		}

		~Cchannel()
		{
			Cmsg out;
			out.event="asterisk_delChannel";
			out.dst=0; 
			out["id"]=id;
			out.send();
		}

	};
	
	
	
	class Cserver
	{
		private:
		CdeviceMap deviceMap;
		CchannelMap channelMap;

		public:	
		string id;
		string username;
		string password;
		int sessionId;

		Cserver()
		{	
		}

	
		CdevicePtr getDevicePtr(string deviceId)
		{
			if (deviceMap[deviceId]==NULL)
			{
				deviceMap[deviceId]=CdevicePtr(new Cdevice());
				deviceMap[deviceId]->setId(deviceId);
				DEB("created device " << deviceId);
			}
			return (deviceMap[deviceId]);
		}
		
		void sendRefresh(int dst)
		{
			//let all devices send a refresh
			for (CdeviceMap::iterator I=deviceMap.begin(); I!=deviceMap.end(); I++)
			{
				I->second->sendRefresh(dst);
			}

			//let all channels send a update
			for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
			{
				I->second->sendRefresh(dst);
			}

		}
	
		CchannelPtr getChannelPtr(string channelId)
		{
			if (channelMap[channelId]==NULL)
			{
				channelMap[channelId]=CchannelPtr(new Cchannel());
				channelMap[channelId]->setId(channelId);
				DEB("created channel " << channelId);
			}
			return (channelMap[channelId]);
		}
	
		void delChannel(string channelId)
		{
			CchannelPtr channelPtr=getChannelPtr(channelId);
			//delete the link, to prevent "hanging" shared_ptrs
			channelPtr->delLink();
			channelMap.erase(channelId);
			DEB("deleted channel " << channelId);

		}


		//on a disconnect this is called to remove all channels/devices.
		void clear()
		{
			//unlink all channels to prevent dangling shared_ptrs
			for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
			{
				I->second->delLink();
			}

			//this should send out automated delChannel/delDevice events, on destruction of the objects
			deviceMap.clear();
		}

		~Cserver()
		{

		}

	};
	
	typedef map<int, Cserver> CserverMap;
	CserverMap serverMap;

}

using namespace ami;



SYNAPSE_REGISTER(ami_Ready)
{
	///if ami is ready, we are ready ;)
	Cmsg out;
	out.clear();
	out.event="core_Ready";
	out.send();
}

/** Connects to specified asterisk server
	\param host Hostname of the asterisk server.
	\param port AMI port (normally 5038)
	\param username Asterisk manager username
	\param password Asterisk manager password

*/
SYNAPSE_REGISTER(asterisk_Connect)
{
	//ami connections are src-session based, so we need a new session for every connection.
	Cmsg out;
	out.event="core_NewSession";
	out["server"]["username"]=msg["username"];
	out["server"]["password"]=msg["password"];
	out["server"]["port"]=msg["port"];
	out["server"]["host"]=msg["host"];
	out.send();
}


SYNAPSE_REGISTER(module_SessionStart)
{

	serverMap[msg.dst].id=msg["server"]["username"].str()+
							"@"+msg["server"]["host"].str()+
							":"+msg["server"]["port"].str();
	
	serverMap[msg.dst].username=msg["server"]["username"].str();
	serverMap[msg.dst].password=msg["server"]["password"].str();

	Cmsg out;
	out.clear();
	out.event="ami_Connect";
	out.src=msg.dst;
	out["host"]=msg["server"]["host"].str();
	out["port"]=msg["server"]["port"].str();
	out.send();
	
}


SYNAPSE_REGISTER(ami_Connected)
{
	//ami is connected, now login with the right credentials for this server:
	Cmsg out;
	out.clear();
	out.src=msg.dst;
	out.event="ami_Action";
	out["Action"]="Login";
	out["UserName"]=serverMap[msg.dst].username;
	out["Secret"]=serverMap[msg.dst].password;
	out["ActionID"]="Login";
	out["Events"]="on";
	out.send();

}

SYNAPSE_REGISTER(ami_Response_Success)
{
	
	//AMI is broken by design: why didnt they just use a Event, instead of defining the format of a Response exception. Now we need to use the ActionID and do extra marshhalling:
	if (msg["ActionID"].str()=="SIPshowPeer")
	{
		//SIPshowPeer response
		string deviceId=msg["Channeltype"].str()+"/"+msg["ObjectName"].str();
		CdevicePtr devicePtr=serverMap[msg.dst].getDevicePtr(deviceId);

		//NOTE: we handle Unmonitored sip peers as online, while we dont actually know if its online or not.		
		if (msg["Status"].str().find("OK")==0 || msg["Status"].str().find("Unmonitored")==0)
			devicePtr->setOnline(true);
		else
			devicePtr->setOnline(false);

		if (msg["Callerid"].str() != "\"\" <>")
			devicePtr->setCallerId(msg["Callerid"].str());
		else
			devicePtr->setCallerId(msg["ObjectName"]);


		devicePtr->sendChanges();
	}
	else if (msg["ActionID"].str()=="Login")
	{
		//login response

		//learn all SIP peers as soon as we login
		Cmsg out;
		out.clear();
		out.src=msg.dst;
		out.event="ami_Action";
		out["Action"]="SIPPeers";
		out.send();

		//learn current channel status as soon as we login
		out.clear();
		out.src=msg.dst;
		out.event="ami_Action";
		out["Action"]="Status";
		out.send();

	}
		
	
}

SYNAPSE_REGISTER(ami_Response_Error)
{
	ERROR("ERROR:" << msg["Message"].str());
	//TODO: pass to clients?

}


SYNAPSE_REGISTER(ami_Disconnected)
{
	//since we're disconnected, clear all devices/channels
	serverMap[msg.dst].clear();
	
	//ami reconnects automaticly
}

SYNAPSE_REGISTER(module_SessionEnd)
{
		
	serverMap[msg.dst].clear();
	serverMap.erase(msg.dst);
}


SYNAPSE_REGISTER(asterisk_refresh)
{
	//indicate start of a refresh, deletes all known state-info in client
	Cmsg out;
	out.event="asterisk_reset";
	out.dst=msg.src; 
	out.send();

	for (CserverMap::iterator I=serverMap.begin(); I!=serverMap.end(); I++)
	{
		I->second.sendRefresh(msg.src);
	}

}


//we got a response to our SIPPeers request.
SYNAPSE_REGISTER(ami_Event_PeerEntry)
{
	serverMap[msg.dst].getDevicePtr(msg["Channeltype"].str()+"/"+msg["ObjectName"].str());

	//request additional device info for this SIP peer
	//example: "SIP/604"
	if (msg["Channeltype"].str()=="SIP")
	{
		//(peerEntrys should be ALWAYS of type SIP?)
		Cmsg out;
		out.clear();
		out.src=msg.dst;
		out.dst=msg.src;
		out.event="ami_Action";
		out["Action"]="SIPshowPeer";
		out["ActionID"]="SIPshowPeer";
		out["Peer"]=msg["ObjectName"].str();
		out.send();
	}

}

void channelStatus(Cmsg & msg)
{
	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Uniqueid"]);
	CdevicePtr devicePtr=serverMap[msg.dst].getDevicePtr(
		getDeviceIdFromChannel(msg["Channel"])
	);

	channelPtr->setDevice(devicePtr);
	channelPtr->setState(msg["State"]);

	//NOTE: whats with all the different namings and <unknown> vs <Unknown> in Newcallerid?

	if (msg.isSet("CallerIDNum"))
	{
		if (msg["CallerIDNum"].str() == "<unknown>")
		 	;//channelPtr->setCallerId("");
		else
		 	channelPtr->setCallerId(msg["CallerIDNum"]);
	}

	if (msg.isSet("CallerID"))
	{
		if (msg["CallerID"].str() == "<unknown>")
		 	;//channelPtr->setCallerId("");
		else
		 	channelPtr->setCallerId(msg["CallerID"]);
	}

	if (msg.isSet("CallerIDName"))
	{
		if (msg["CallerIDName"].str() == "<unknown>")
		 	channelPtr->setCallerIdName("");
		else
			channelPtr->setCallerIdName(msg["CallerIDName"]);
	}

	devicePtr->sendChanges();
	channelPtr->sendChanges();
	channelPtr->sendDebug(msg, msg.dst);

	
}


// new channel created
SYNAPSE_REGISTER(ami_Event_Newchannel)
{

/*	Event: Newchannel
	Privilege: call,all
	Channel: SIP/604-00000069
	State: Down
	CallerIDNum: 604
	CallerIDName: Edwin (draadloos)
	Uniqueid: 1269871368.143

	Event: Newchannel
	Privilege: call,all
	Channel: SIP/605-0000006a
	State: Down
	CallerIDNum: <unknown>
	CallerIDName: <unknown>
	Uniqueid: 1269871368.144*/

	channelStatus(msg);
}

// initial channel status event, requested just after connecting to an asterisk server.
SYNAPSE_REGISTER(ami_Event_Status)
{
// 	Event: Status
// 	Privilege: Call
// 	Channel: SIP/601-00000048
// 	CallerID: 601
// 	CallerIDNum: 601
// 	CallerIDName: <unknown>
// 	Account:
// 	State: Ringing
// 	Uniqueid: 1269958019.99
// 
// 	Event: Status
// 	Privilege: Call
// 	Channel: SIP/605-00000047
// 	CallerID: 605
// 	CallerIDNum: 605
// 	CallerIDName: <unknown>
// 	Account:
// 	State: Up
// 	Link: SIP/604-00000046
// 	Uniqueid: 1269958018.98


	channelStatus(msg);

}

// channel status is changing
SYNAPSE_REGISTER(ami_Event_Newstate)
{
	/*
	|CallerID = 605 (string)
	|CallerIDName = <unknown> (string)
	|Channel = SIP/605-0000005b (string)
	|Event = Newstate (string)
	|Privilege = call,all (string)
	|State = Ringing (string)
	|Uniqueid = 1269870185.119 (string)                                                                                                     */

	channelStatus(msg);
}


SYNAPSE_REGISTER(ami_Event_Hangup)
{
	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Uniqueid"]);
	channelPtr->sendDebug(msg, msg.dst);
	serverMap[msg.dst].delChannel(msg["Uniqueid"]);
//	serverMap[msg.dst].getDevicePtr(getDeviceId(msg["Channel"]))->sendStatus();
//	INFO("\n" << serverMap[msg.dst].getStatusStr());

}

SYNAPSE_REGISTER(ami_Event_PeerStatus)
{
	
	CdevicePtr devicePtr=serverMap[msg.dst].getDevicePtr(msg["Peer"]);
	if (msg["PeerStatus"].str()=="Reachable" || msg["PeerStatus"].str()=="Registered" )
		devicePtr->setOnline(true);
	else
		devicePtr->setOnline(false);

	devicePtr->sendChanges();

}



SYNAPSE_REGISTER(ami_Event_Link)
{
/*	Event: Link
	Privilege: call,all
	Channel1: SIP/604-00000022
	Channel2: SIP/605-00000023
	Uniqueid1: 1269864649.42
	Uniqueid2: 1269864649.43
	CallerID1: 604
	CallerID2: 605*/
	CchannelPtr channelPtr1=serverMap[msg.dst].getChannelPtr(msg["Uniqueid1"]);
	CchannelPtr channelPtr2=serverMap[msg.dst].getChannelPtr(msg["Uniqueid2"]);

	channelPtr1->setLink(channelPtr2);
	channelPtr2->setLink(channelPtr1);

	//this will automagically send updates to BOTH channels, sinces they're linked now:
	channelPtr1->sendChanges();

	channelPtr1->sendDebug(msg, msg.dst);
	channelPtr2->sendDebug(msg, msg.dst);
}


SYNAPSE_REGISTER(ami_Event_Unlink)
{
	/*Event: Unlink
	Privilege: call,all
	Channel1: SIP/604-00000020
	Channel2: SIP/605-00000021
	Uniqueid1: 1269864479.40
	Uniqueid2: 1269864479.41
	CallerID1: 604
	CallerID2: 605*/
	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Uniqueid1"]);
	channelPtr->delLink();
	channelPtr->sendChanges();
	channelPtr->sendDebug(msg, msg.dst);

	channelPtr=serverMap[msg.dst].getChannelPtr(msg["Uniqueid2"]);
	channelPtr->delLink();
	channelPtr->sendChanges();
	channelPtr->sendDebug(msg, msg.dst);



}


SYNAPSE_REGISTER(ami_Event_Newexten)
{
// Event: Newexten
// Privilege: call,all
// Channel: SIP/604-00000007
// Context: DLPN_DatuX
// Extension: 9991234
// Priority: 1
// Application: Macro
// AppData: trunkdial-failover-0.3|SIP/0858784323/9991234|mISDN/g:trunk_m1/9991234|0858784323|trunk_m1
// Uniqueid: 1269802539.12

/* |AppData = SIP/605 (string)
 |Application = Dial (string)
 |Channel = SIP/604-0000002f (string)
 |Context = DLPN_DatuX (string)
 |Event = Newexten (string)
 |Extension = 605 (string)
 |Priority = 1 (string)
 |Privilege = call,all (string)
 |Uniqueid = 1269866053.55 (string)*/


	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Uniqueid"]);
	if (channelPtr->getFirstExtension()=="")
	{
		channelPtr->setFirstExtension(msg["Extension"]);
		channelPtr->sendChanges();
	}

	channelPtr->sendDebug(msg, msg.dst);



}


// 
// SYNAPSE_REGISTER(ami_Event_ExtensionStatus)
// {

// }
// 

SYNAPSE_REGISTER(ami_Event_Dial)
{
// 	Event: Dial
// 	Privilege: call,all
// 	Source: SIP/604-0000002f
// 	Destination: SIP/605-00000030
// 	CallerID: 604
// 	CallerIDName: Edwin (draadloos)
// 	SrcUniqueID: 1269866053.55
// 	DestUniqueID: 1269866053.56

// 	Event: Dial
// 	Privilege: call,all
// 	Source: SIP/604-00000031
// 	Destination: mISDN/0-u5
// 	CallerID: 0858784323
// 	CallerIDName: <unknown>
// 	SrcUniqueID: 1269866267.57
// 	DestUniqueID: 1269866267.58


	//NOTE: a "link" for us, is something different then a link for asterisk.
	CchannelPtr channelPtr1=serverMap[msg.dst].getChannelPtr(msg["SrcUniqueID"]);
	CchannelPtr channelPtr2=serverMap[msg.dst].getChannelPtr(msg["DestUniqueID"]);
	channelPtr1->setLink(channelPtr2);
	channelPtr2->setLink(channelPtr1);

	channelPtr1->setInitiator(true);
	channelPtr2->setInitiator(false);
	
	//in case of followme and other situation its important we use the Dial callerId as well, for SrcUniqueID:
	if (msg["CallerID"].str() == "<unknown>")
	 	;//channelPtr->setCallerId("");
	else
	 	channelPtr1->setCallerId(msg["CallerID"]);

	if (msg["CallerIDName"].str() == "<unknown>")
	 	channelPtr1->setCallerIdName("");
	else
		channelPtr1->setCallerIdName(msg["CallerIDName"]);



	//this will automagically send updates to BOTH channels, sinces they're linked now:
	channelPtr1->sendChanges();
	channelPtr1->sendDebug(msg, msg.dst);
	channelPtr2->sendDebug(msg, msg.dst);

/*
	channelPtr=serverMap[msg.dst].getChannelPtr(msg["DestUniqueID"]);
	channelPtr->setLink(msg["SrcUniqueID"]);*/

// 	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Destination"]);
// 	channelPtr->setCallerId("gettingcalled\"" + msg["CallerIDName"].str() + "\" <" + msg["CallerID"].str() + ">" );

// 	channelPtr=serverMap[msg.dst].getChannelPtr(msg["Source"]);
// 	channelPtr->setCallerId("calling\"" + msg["CallerIDName"].str() + "\" <" + msg["CallerID"].str() + ">" );

}

// renaming usually happens on things like call-transfers

SYNAPSE_REGISTER(ami_Event_Rename)
{
	// Event: Rename
	// Privilege: call,all
	// Oldname: SIP/604-00000044
	// Newname: SIP/605-00000043
	// Uniqueid: 1269956933.94
	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Uniqueid"]);
	CdevicePtr devicePtr=serverMap[msg.dst].getDevicePtr(
		getDeviceIdFromChannel(msg["Newname"])
	);

	//we assume a rename only is possible for channels that are already up?
	channelPtr->setState("Up");
	channelPtr->setDevice(devicePtr);

	if (channelPtr->getLink()==CchannelPtr())
	{
		//when we get renamed while NOT linked, we store the current callerids in the linkedcallerids.
		//after the rename we usually receive our new caller id immeadiatly
		channelPtr->setLinkCallerId(channelPtr->getCallerId());
		channelPtr->setLinkCallerIdName(channelPtr->getCallerIdName());
	}

	channelPtr->sendChanges();
	channelPtr->sendDebug(msg, msg.dst);
}

SYNAPSE_REGISTER(ami_Event_Newcallerid)
{
// 	Event: Newcallerid
// 	Privilege: call,all
// 	Channel: SIP/605-00000033
// 	CallerID: 605
// 	CallerIDName: <Unknown>
// 	Uniqueid: 1269866486.60
// 	CID-CallingPres: 0 (Presentation Allowed, Not Screened)

// Event: Newcallerid
// Privilege: call,all
// Channel: mISDN/0-u5
// CallerID: 0622588835
// CallerIDName: <Unknown>
// Uniqueid: 1269866267.58
// CID-CallingPres: 0 (Presentation Allowed, Not Screened)

// Event: Newcallerid
// Privilege: call,all
// Channel: SIP/604-00000031
// CallerID: 0858784323
// CallerIDName: <Unknown>
// Uniqueid: 1269866267.57
// CID-CallingPres: 0 (Presentation Allowed, Not Screened)

	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Uniqueid"]);

	if (msg["CallerID"].str() == "<Unknown>")
	 	;//channelPtr->setCallerId("");
	else
	 	channelPtr->setCallerId(msg["CallerID"]);

	if (msg["CallerIDName"].str() == "<Unknown>")
	 	channelPtr->setCallerIdName("");
	else
	 	channelPtr->setCallerIdName(msg["CallerIDName"]);
	
	channelPtr->sendChanges();
	channelPtr->sendDebug(msg, msg.dst);

}


// 
// 
// SYNAPSE_REGISTER(ami_Event_PeerStatus)
// {

// 
// }


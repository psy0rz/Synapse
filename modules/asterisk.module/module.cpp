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

	string getDeviceId(string channelId)
	{
		smatch what;
		if (!regex_search(
			channelId,
			what, 
			boost::regex("^(.*)-([^-]*)$")
		))
		{
			WARNING("Invalid channelId: " << channelId << ", using 'unknown' instead");
			return (string("unknown"));
		}
		else
		{
			return (what[1]);
		}
	}

	
	class Cchannel
	{
		private:

		string id;
		bool changed;
		string state;
		string linkId;
		string callerIdLink;
		string callerId;
		bool incoming;

		public:

		Cchannel()
		{
			changed=true;
			incoming=false;
		}

		void setId(string id)
		{
			if (id!=this->id)
			{
				this->id=id;
				changed=true;
			}
			sendUpdate();
		}

		void setLink(string channelId)
		{
			if (channelId!=this->linkId)
			{
				this->linkId=channelId;
				changed=true;
			}
			sendUpdate();

		}

		string getLink()
		{
			return(linkId);
		}

		void setCallerIdLink(string callerIdLink)
		{
			if (callerIdLink!=this->callerIdLink)
			{
				this->callerIdLink=this->callerIdLink;
				changed=true;
			}
			sendUpdate();
		}

		void setCallerId(string callerId)
		{
			if (callerId!=this->callerId)
			{
				this->callerId=callerId;
				changed=true;
			}
			sendUpdate();

		}

		void setState(string state)
		{
			if (state=="Ring")
				incoming=false;
			else if (state=="Ringing")
				incoming=true;
	
			if (state!=this->state)
			{
				this->state=state;
				changed=true;
			}
			sendUpdate();
		}

		void sendUpdate(int forceDst=0)
		{
			if (changed || forceDst)
			{	
				Cmsg out;
				out.event="asterisk_updateChannel";
				out.dst=forceDst;
				out["id"]=id;
				out["deviceId"]=getDeviceId(id);
				out["state"]=state;
				out["linkId"]=linkId;
				out["incoming"]=incoming;
				out["callerId"]=callerId;
				out["callerIdLink"]=callerIdLink;
				out.send();
			}

			if (!forceDst)
				changed=false;
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
	typedef shared_ptr<Cchannel> CchannelPtr;
	typedef map<string, CchannelPtr> CchannelMap;
	
	
	class Cdevice
	{
		private: 
		CchannelMap channelMap;

		bool online;
		string callerId;
		string id;
		bool changed;
	
		public:

		Cdevice()
		{
			changed=true;
			id="";
			online=false;
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
			sendUpdate();
		}
	
		void setCallerId(string callerId)
		{
			if (callerId!="" && callerId!=this->callerId)
			{
				this->callerId=callerId;
				changed=true;
			}
			sendUpdate();
		}

		void setOnline(bool online)
		{
			if (online!=this->online)
			{
				this->online=online;
				changed=true;
			}
			sendUpdate();
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
			channelMap.erase(channelId);
			DEB("deleted channel " << channelId);

		}
	
// 		string getStatusStr()
// 		{
// 			stringstream s;
// 			for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
// 			{
// 				if (I->second !=NULL)
// 				{
// 					s << "  Channel " << I->second->id << "\n";
// 				}
// 				else
// 				{
// 					s << "  Channel NULL? :" << I->first << "\n";
// 				}
// 			}
// 			return (s.str());
// 		}

		void sendUpdate(int forceDst=0)
		{
			if (changed || forceDst)
			{	
				Cmsg out;
				out.event="asterisk_updateDevice";
				out.dst=forceDst;
				out["id"]=id;
				out["callerId"]=callerId;
				out["online"]=online;
				out.send();
			}

			if (!forceDst)
				changed=false;

		}


		void sendRefresh(int dst)
		{
			//refresh device
			sendUpdate(dst);

			//let all channels send a update
			for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
			{
				I->second->sendRefresh(dst);
			}
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
	
	typedef shared_ptr<Cdevice> CdevicePtr;
	typedef map<string, CdevicePtr> CdeviceMap;
	
	
	
	class Cserver
	{
		CdeviceMap deviceMap;

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
		}
	
/*		string getStatusStr(bool verbose=false)
		{
			stringstream s;
			s << "Server " << id << ":\n";
			for (CdeviceMap::iterator I=deviceMap.begin(); I!=deviceMap.end(); I++)
			{
				if (I->second !=NULL)
				{
					//filter idle devices
					string devStr=I->second->getStatusStr();
					if (verbose || devStr!="")
					{
						s << " Device " << I->second->id << ":\n";
						s << devStr;
					}
				}
				else
				{
					s << " Device NULL? :" << I->first << "\n";
				}
			}
			return (s.str());
		}*/
	
		CchannelPtr getChannelPtr(string channelId)
		{
			string deviceId;
			deviceId=getDeviceId(channelId);
			
			CdevicePtr devicePtr=getDevicePtr(deviceId);
	
			return(devicePtr->getChannelPtr(channelId));
		}

		void delChannel(string channelId)
		{
			string deviceId;
			deviceId=getDeviceId(channelId);
			
			CdevicePtr devicePtr=getDevicePtr(deviceId);
			devicePtr->delChannel(channelId);
		}

		//on a disconnect this is called to remove all channels/devices.
		void clear()
		{
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

		devicePtr->setCallerId(msg["Callerid"].str());
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



void ChannelStatus(Cmsg & msg)
{
	Cmsg out;
	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Channel"]);
	channelPtr->setState(msg["State"]);
}

SYNAPSE_REGISTER(ami_Event_Status)
{
	ChannelStatus(msg);

}

SYNAPSE_REGISTER(ami_Event_Newchannel)
{
	ChannelStatus(msg);
}

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


	ChannelStatus(msg);
}


SYNAPSE_REGISTER(ami_Event_Hangup)
{
	serverMap[msg.dst].delChannel(msg["Channel"]);
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
	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Channel1"]);
	channelPtr->setLink(msg["Channel2"]);

	channelPtr=serverMap[msg.dst].getChannelPtr(msg["Channel2"]);
	channelPtr->setLink(msg["Channel1"]);

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
	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Channel1"]);
	channelPtr->setLink("");

	channelPtr=serverMap[msg.dst].getChannelPtr(msg["Channel2"]);
	channelPtr->setLink("");

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


// 	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Channel"]);
// 	channelPtr->setCallingTo(msg["Extension"]);

	if (msg["Extension"].str() == "9991234" )
	{
/*		Cmsg out;
		out.event="ami_Action";
		out.dst=msg.src;
		out.src=msg.dst;
		out["Action"]="Hangup";
		out["Channel"]=msg["Channel"].str();
		out.send();*/
	}


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
	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Source"]);
	channelPtr->setLink(msg["Destination"]);

	channelPtr=serverMap[msg.dst].getChannelPtr(msg["Destination"]);
	channelPtr->setLink(msg["Source"]);

// 	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Destination"]);
// 	channelPtr->setCallerId("gettingcalled\"" + msg["CallerIDName"].str() + "\" <" + msg["CallerID"].str() + ">" );

// 	channelPtr=serverMap[msg.dst].getChannelPtr(msg["Source"]);
// 	channelPtr->setCallerId("calling\"" + msg["CallerIDName"].str() + "\" <" + msg["CallerID"].str() + ">" );

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

	string callerId="\"" + msg["CallerIDName"].str() + "\" <" + msg["CallerID"].str() + ">";

 	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Channel"]);
 	channelPtr->setCallerId(callerId);

	//do we know the channel we are Dialing to? if so, set the callerIdLink of that channel so we can show who's calling.
	if (channelPtr->getLink()!="")
	{
		channelPtr=serverMap[msg.dst].getChannelPtr(channelPtr->getLink());
		channelPtr->setCallerIdLink(callerId);
	}
}


// 
// 
// SYNAPSE_REGISTER(ami_Event_PeerStatus)
// {

// 
// }


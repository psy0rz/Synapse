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

	//FIXME
	out.clear();
	out.event="core_ChangeEvent";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"anonymous";


	out["event"]=		"asterisk_addChannel"; 
	out.send();

	out["event"]=		"asterisk_addDevice"; 
	out.send();

	out["event"]=		"asterisk_delChannel"; 
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
		public:
		string id;

		void sendAdd(int dst)
		{
			Cmsg out;
			out.event="asterisk_addChannel";
			out.dst=dst;
			out["id"]=id;
			out["deviceId"]=getDeviceId(id);
			out.send();
		}

		void sendRefresh(int dst)
		{
			sendAdd(dst);
		}

	};
	typedef shared_ptr<Cchannel> CchannelPtr;
	typedef map<string, CchannelPtr> CchannelMap;
	
	
	class Cdevice
	{
		private: 
		CchannelMap channelMap;
	
		public:
		string id;
	
		CchannelPtr getChannelPtr(string channelId)
		{
			if (channelMap[channelId]==NULL)
			{
				channelMap[channelId]=CchannelPtr(new Cchannel());
				channelMap[channelId]->id=channelId;
				DEB("created channel " << channelId);

				channelMap[channelId]->sendAdd(0);				

			}
			return (channelMap[channelId]);
		}
	
		void delChannel(string channelId)
		{
			channelMap.erase(channelId);
			DEB("removed channel " << channelId);

			Cmsg out;
			out.event="asterisk_delChannel";
			out.dst=0; //FIXME
			out["id"]=channelId;
			out.send();
		}
	
		string getStatusStr()
		{
			stringstream s;
			for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
			{
				if (I->second !=NULL)
				{
					s << "  Channel " << I->second->id << "\n";
				}
				else
				{
					s << "  Channel NULL? :" << I->first << "\n";
				}
			}
			return (s.str());
		}

		void sendAdd(int dst)
		{
			Cmsg out;
			out.event="asterisk_addDevice";
			out.dst=dst;
			out["id"]=id;
			out.send();
		}

		void sendRefresh(int dst)
		{
			//refresh device
			sendAdd(dst);

			//let all channels send their refresh info
			for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
			{
				I->second->sendRefresh(dst);
			}

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

	
		CdevicePtr getDevicePtr(string deviceId)
		{
			if (deviceMap[deviceId]==NULL)
			{
				deviceMap[deviceId]=CdevicePtr(new Cdevice());
				deviceMap[deviceId]->id=deviceId;
				DEB("created device " << deviceId);

				deviceMap[deviceId]->sendAdd(0);		

			}
			return (deviceMap[deviceId]);
		}
		
		void sendRefresh(int dst)
		{
			//let all devices send their refresh info
			for (CdeviceMap::iterator I=deviceMap.begin(); I!=deviceMap.end(); I++)
			{
				I->second->sendRefresh(dst);
			}
		}
	
		string getStatusStr(bool verbose=false)
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
		}
	
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

	//te snel, pas doen na login succes!
	//learn all SIP peers as soon as we connect
/*	out.clear();
	out.src=msg.dst;
	out.event="ami_Action";
	out["Action"]="SIPPeers";
	out.send();*/
}

SYNAPSE_REGISTER(ami_Response_Success)
{
	
/*	if (msg["Action"]=="Login")
	{*/
/*	}*/
		
	
}

SYNAPSE_REGISTER(ami_Response_Error)
{
	ERROR("ERROR");

}


SYNAPSE_REGISTER(ami_Disconnected)
{
	
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
	out.dst=dst; 
	out.send();

	for (CserverMap::iterator I=serverMap.begin(); I!=serverMap.end(); I++)
	{
		I->second.sendRefresh(msg.src);
	}

}

SYNAPSE_REGISTER(ami_Event_PeerEntry)
{
	serverMap[msg.dst].getDevicePtr(msg["Channeltype"].str()+"/"+msg["ObjectName"].str());
//	INFO("\n" << serverMap[msg.dst].getStatusStr());

}


SYNAPSE_REGISTER(ami_Event_Newchannel)
{
	
	serverMap[msg.dst].getChannelPtr(msg["Channel"]);
//	serverMap[msg.dst].getDevicePtr(getDeviceId(msg["Channel"]))->sendStatus();

//	INFO("NEWCHANNEL UPDATE:\n" << serverMap[msg.dst].getStatusStr());
	
	//serverMap[msg.dst].sendStatus();
}

SYNAPSE_REGISTER(ami_Event_Hangup)
{
	serverMap[msg.dst].delChannel(msg["Channel"]);
//	serverMap[msg.dst].getDevicePtr(getDeviceId(msg["Channel"]))->sendStatus();
//	INFO("\n" << serverMap[msg.dst].getStatusStr());

}

SYNAPSE_REGISTER(ami_Event_PeerStatus)
{
	
	serverMap[msg.dst].getDevicePtr(msg["Peer"]);
//	INFO("\n" << serverMap[msg.dst].getStatusStr());
}


// SYNAPSE_REGISTER(ami_Event_Newstate)
// {

// 
// }
// 
// SYNAPSE_REGISTER(ami_Event_Newexten)
// {

// 
// }
// 
// SYNAPSE_REGISTER(ami_Event_ExtensionStatus)
// {

// }
// 
// SYNAPSE_REGISTER(ami_Event_Dial)
// {

// 
// }
// 
// SYNAPSE_REGISTER(ami_Event_Newcallerid)
// {

// 
// }
// 
// 
// SYNAPSE_REGISTER(ami_Event_Link)
// {
// 

// }
// 
// SYNAPSE_REGISTER(ami_Event_Unlink)
// {

// 
// }
// 
// 
// SYNAPSE_REGISTER(ami_Event_PeerStatus)
// {

// 
// }


/** \file
Asterisk Control Module.

Uses the AMI module (and maybe others) to track and control asterisk.
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


}




namespace ami
{
	
	class Cchannel
	{
		public:
		string name;
	};
	typedef shared_ptr<Cchannel> CchannelPtr;
	typedef map<string, CchannelPtr> CchannelMap;
	
	
	class Cdevice
	{
		private: 
		CchannelMap channelMap;
	
		public:
		string name;
	
		CchannelPtr getChannelPtr(string channelId)
		{
			if (channelMap[channelId]==NULL)
			{
				channelMap[channelId]=CchannelPtr(new Cchannel());
				channelMap[channelId]->name=channelId;
				DEB("created channel " << channelId);
			};
			return (channelMap[channelId]);
		}
	
		void delChannel(string channelId)
		{
			if (channelMap[channelId]!=NULL)
			{
				channelMap.erase(channelId);
				DEB("removed channel " << channelId);
			}
		}
	
		string getStatusStr()
		{
			stringstream s;
			for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
			{
				s << "  Channel " << I->second->name << "\n";
			}
			return (s.str());
		}
	};
	
	typedef shared_ptr<Cdevice> CdevicePtr;
	typedef map<string, CdevicePtr> CdeviceMap;
	
	
	
	class Cserver
	{
		CdeviceMap deviceMap;

		public:	
		string name;
		string username;
		string password;

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
	
		CdevicePtr getDevicePtr(string deviceId)
		{
			if (deviceMap[deviceId]==NULL)
			{
				deviceMap[deviceId]=CdevicePtr(new Cdevice());
				deviceMap[deviceId]->name=deviceId;
				DEB("created device " << deviceId);
			}
			return (deviceMap[deviceId]);
		}
		
	
		string getStatusStr()
		{
			stringstream s;
			s << "Server " << name << ":\n";
			for (CdeviceMap::iterator I=deviceMap.begin(); I!=deviceMap.end(); I++)
			{
				s << " Device " << I->second->name << ":\n";
				s << I->second->getStatusStr();
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
	

	map<int, Cserver> servers;

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

	servers[msg.dst].name=msg["server"]["username"].str()+
							"@"+msg["server"]["host"].str()+
							":"+msg["server"]["port"].str();
	
	servers[msg.dst].username=msg["server"]["username"].str();
	servers[msg.dst].password=msg["server"]["password"].str();

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
	out["UserName"]=servers[msg.dst].username;
	out["Secret"]=servers[msg.dst].password;
	out["ActionID"]="Login";
	out["Events"]="on";
	out.send();
}

SYNAPSE_REGISTER(ami_Disconnected)
{
	
	//ami reconnects automaticly
}

SYNAPSE_REGISTER(module_SessionEnd)
{
	
	servers.erase(msg.dst);
}

SYNAPSE_REGISTER(ami_Response_Success)
{
	
	
}

SYNAPSE_REGISTER(ami_Event_Newchannel)
{
	
	servers[msg.dst].getChannelPtr(msg["Channel"]);
	INFO("\n" << servers[msg.dst].getStatusStr());
}

SYNAPSE_REGISTER(ami_Event_Hangup)
{
	servers[msg.dst].delChannel(msg["Channel"]);
	INFO("\n" << servers[msg.dst].getStatusStr());

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


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


/** \file
Asterisk Control Module.

Uses the AMI module (and maybe others) to track and control asterisk.

The complex flow of asterisk events is converted to a few simple events, usable in operator panels and other apps.

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

/*
    Data structure overview:

    groupMap->groupPtr------------------>Cgroup
                                          ^ ^
    sessionMap->sessionPtr->Csession:     | |
        groupPtr--->----------------------  |
                                            |
    serverMap->serverPtr->Cserver:          |
        channelMap->channelPtr->Cchannel:   |
            devicePtr--->-------\           |
                                 v          |
        deviceMap->devicePtr->Cdevice:      |
            groupPtr--->--------------------

*/

#include "synapse.h"
#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include "./csession.h"
#include "cconfig.h"
//#include "./cgroup.h"
//#include "./csession_types.h"
//#include "./cgroup_types.h"

#define ASTERISK_AUTH "9999"

namespace asterisk
{
	using namespace std;
	using namespace boost;

	CsessionMap sessionMap;

	CgroupMap groupMap;

	typedef shared_ptr<class Cdevice> CdevicePtr;
	typedef map<string, CdevicePtr> CdeviceMap;

	typedef shared_ptr<class Cchannel> CchannelPtr;
	typedef map<string, CchannelPtr> CchannelMap;

	typedef map<int, class Cserver> CserverMap;
	CserverMap serverMap;

	typedef long int TauthCookie;
 	struct drand48_data randomBuffer;


 	//authentication cookies and recent call history are saved to disk
 	synapse::Cconfig stateDb;


	SYNAPSE_REGISTER(module_Init)
	{
		Cmsg out;


		stateDb.load("var/asterisk/state_db.conf");


		//FIXME: unsafe randomiser?
		srand48_r(time(NULL), &randomBuffer);


	
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
	
	
		out.clear();
		out.event="core_LoadModule";
		out["path"]="modules/timer.module/libtimer.so";
		out.send();


		out.clear();
		out.event="core_LoadModule";
		out["path"]="modules/http_json.module/libhttp_json.so";
		out.send();

	
		out.clear();


		//send by anonymous only:
		out.event="core_ChangeEvent";
		out["modifyGroup"]=	"modules";
		out["sendGroup"]=	"anonymous";
		out["recvGroup"]=	"modules";

		out["event"]=		"asterisk_authReq";
		out.send();

		out["event"]=		"asterisk_refresh";
		out.send();

		//receive by anonymous only:
		out.event="core_ChangeEvent";
		out["modifyGroup"]=	"modules";
		out["sendGroup"]=	"modules";
		out["recvGroup"]=	"anonymous";

	
	 	out["event"]=		"asterisk_debugChannel"; 
 	 	out.send();
	

		out["event"]=		"asterisk_authCall"; 
		out.send();
	
		out["event"]=		"asterisk_authOk"; 
		out.send();
	
		out["event"]=		"asterisk_updateChannel"; 
		out.send();
	
		out["event"]=		"asterisk_delChannel"; 
		out.send();
	
		out["event"]=		"asterisk_updateDevice"; 
		out.send();
	
		out["event"]=		"asterisk_delDevice"; 
		out.send();

		out["event"]=		"asterisk_reset"; 
		out.send();
	
	}


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

	//devices: these can be sip devices, misdn, local channels, agent-stuff etc
	//every device points to a corresponding Cgroup. 
	class Cdevice
	{
		private: 

		bool trunk;
		bool online;
		string callerId;
		string callerIdName;
		string id;
		bool changed;
		CgroupPtr groupPtr;	
		//TauthCookie authCookie;
	
		public:

		Cdevice()
		{
			

			changed=true;
			online=true;
			trunk=true; //assume true, to prevent showing trunks on asterisk-reconnect
		}

		//check if the device should be filtered from the endusers, according to various filtering options
		bool isFiltered()
		{
			//dont show trunks
			// if (trunk)
			// 	return (true);

			//dont show devices with empty caller ID's
			// if (callerIdName=="")
			// 	return (true);

			//dont show anything thats not sip (for now)
			// if (id.substr(0,3)!="SIP")
			// 	return (true);

			return(false);
		}

		CgroupPtr getGroupPtr()
		{
			return (groupPtr);
		}

		TauthCookie getAuthCookie()
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


		void setId(string id)
		{
			if (id!=this->id)
			{
				this->id=id;
				if (callerId=="")
				{
					callerId=id;
				}

				//now we determine the corresponding group from the device Id string:
				size_t pos=id.find("-");
				string groupId;

				if (pos!=string::npos)
					groupId=id.substr(pos+1);

				if (groupId=="")
					groupId="default";

				if (groupMap.find(groupId) == groupMap.end())
				{
					groupMap[groupId]=CgroupPtr(new Cgroup());
					groupMap[groupId]->setId(groupId);
				}
				groupPtr=groupMap[groupId];

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

		string getCallerId()
		{
			return(callerId);
		}

		void setCallerIdName(string callerIdName)
		{
			if (callerIdName!="" && callerIdName!=this->callerIdName)
			{
				this->callerIdName=callerIdName;
				changed=true;
			}
		}

		string getCallerIdName()
		{
			return(callerIdName);
		}


		void setTrunk(bool trunk)
		{
			this->trunk=trunk;
			changed=true;
		}

		void setOnline(bool online)
		{
			if (online!=this->online)
			{
				this->online=online;
				changed=true;
			}
		}


	

		bool sendUpdate(int forceDst=0)
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
				out["groupId"]=groupPtr->getId();
				groupPtr->send(sessionMap,out);
			}

			return(true);
		}


		void sendChanges()
		{
			if (changed)
			{	
				if (sendUpdate())
					changed=false;
			}
		}


		void sendRefresh(int dst)
		{
			//refresh device
			sendUpdate(dst);
		}

		string getStatus(string prefix)
		{
			return (
				prefix+"Device "+id+":\n"+
				groupPtr->getStatus(prefix+" ")
			);
		}


		~Cdevice()
		{
			Cmsg out;
			out.event="asterisk_delDevice";
			out.dst=0; 
			out["id"]=id;

			if (groupPtr!=NULL)
				groupPtr->send(sessionMap,out);
		}

		
	};
	
	//asterisk channels. these always point to a corresponding Cdevice
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
//		bool initiator;
		CdevicePtr devicePtr;

		int changesSent;		

		public:

		Cchannel()
		{
			changes=1;
			changesSent=0;
//			initiator=true;
		}

		bool sendDebug(Cmsg msg, int serverId)
		{
			if (devicePtr==NULL || devicePtr->getGroupPtr()==NULL)
				return (false);

			msg.event="asterisk_debugChannel";
			msg["serverId"]=serverId;
			msg["id"]=id;
			if (devicePtr!=CdevicePtr())
			{
				msg["deviceId"]=devicePtr->getId();
			}
			msg.dst=0;
			msg.src=0;

			devicePtr->getGroupPtr()->send(sessionMap,msg);
			return (true);
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

//		void setInitiator(bool initiator)
//		{
//			if (initiator!=this->initiator)
//			{
//				this->initiator=initiator;
//				changes++;
//			}
//
//		}

		void setDevicePtr(CdevicePtr devicePtr)
		{
			if (devicePtr!=this->devicePtr)
			{
				if (
						//device disappeared somehow?
						(this->devicePtr && devicePtr) ||
						//or channel moved to other device?
						(this->devicePtr && devicePtr && this->devicePtr->getGroupPtr()!=devicePtr->getGroupPtr())
				)
				{
					//the channel is moving to another group.
					//send a delete to the group of the current device:
					//(otherwise the people in the current group wont see the delChannel lateron)
					if (this->devicePtr->getGroupPtr())
					{
						Cmsg out;
						out.event="asterisk_delChannel";
						out.dst=0;
						out["id"]=id;
						if (this->devicePtr!=NULL)
						{
							out["deviceId"]=this->devicePtr->getId();
						}
						this->devicePtr->getGroupPtr()->send(sessionMap,out);
					}
				}

				//store the new device
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

		void setLinkPtr(CchannelPtr channelPtr)
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
			setLinkPtr(CchannelPtr());
		}

		CchannelPtr getLinkPtr()
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

		string getState()
		{
			return (state);
		}
	
		void setState(string state)
		{
	
			if (state!=this->state)
			{
				this->state=state;
				changes++;
			}
		}

		void sendChanges()
		{

			// are there changes we didnt send yet?
			if (changes>changesSent)
			{
				if (sendUpdate())
					changesSent=changes;
			}	


		}

		bool sendUpdate(int forceDst=0)
		{
			if (devicePtr==NULL || devicePtr->getGroupPtr()==NULL)
				return (false);

			if (devicePtr->isFiltered())
				return (false);

			Cmsg out;
			out.event="asterisk_updateChannel";
			out.dst=forceDst;
			out["id"]=id;
			out["state"]=state;
//			out["initiator"]=initiator;
			out["callerId"]=callerId;
			out["callerIdName"]=callerIdName;

			if (devicePtr!=CdevicePtr())
			{
				out["deviceId"]=devicePtr->getId();
				out["deviceCallerId"]=devicePtr->getCallerId();
				out["deviceCallerIdName"]=devicePtr->getCallerIdName();
			}

			out["linkCallerId"]=linkCallerId;
			out["linkCallerIdName"]=linkCallerIdName;

			out["firstExtension"]=firstExtension;

			devicePtr->getGroupPtr()->send(sessionMap,out);

			return (true);
		}

		void sendRefresh(int dst)
		{
			sendUpdate(dst);
		}

		string getStatus(string prefix)
		{
			return (
				prefix + "Channel " + id + ":\n" +
				devicePtr->getStatus(prefix+" ")
			);
		}

		~Cchannel()
		{

			if (devicePtr!=NULL && devicePtr->getGroupPtr()!=NULL)
			{
				Cmsg out;
				out.event="asterisk_delChannel";
				out.dst=0; 
				out["id"]=id;
				if (devicePtr!=CdevicePtr())
				{
					out["deviceId"]=devicePtr->getId();
				}
				devicePtr->getGroupPtr()->send(sessionMap,out);
			}
		}

	};
	
	
	//physical asterisk servers. every server has its own device and channel map
	class Cserver
	{
		private:
		CdeviceMap deviceMap;
		CchannelMap channelMap;

		public:	
		enum Estatus
		{
			CONNECTING,
			AUTHENTICATING,
			AUTHENTICATED,
		};

		Estatus status;

		string id;
		string username;
		string password;
		string host;
		string port;

		int sessionId;

		Cserver()
		{
			status=CONNECTING;
		}

	
		CdevicePtr getDevicePtr(string deviceId, bool autoCreate=true)
		{
			if (deviceMap.find(deviceId)==deviceMap.end())
			{
				if (autoCreate)
				{
					deviceMap[deviceId]=CdevicePtr(new Cdevice());
					deviceMap[deviceId]->setId(deviceId);
					DEB("created device " << deviceId);
				}
				else
				{
					return (CdevicePtr());
				}
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

		void sendChanges()
		{
			//let all devices send their changes, if they have them.
			for (CdeviceMap::iterator I=deviceMap.begin(); I!=deviceMap.end(); I++)
			{
				I->second->sendChanges();
			}

			//let all channels send their changes, if they have them.
			for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
			{
				I->second->sendChanges();
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
			DEB("deleted channel from map: " << channelId);

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
			channelMap.clear();
		}


		string getStatus(string prefix)
		{
			string s;
			s=prefix+"Server "+id+": ";



			if (status==CONNECTING)
				s=s+"connecting...";
			if (status==AUTHENTICATING)
				s=s+"authenticating...";
			if (status==AUTHENTICATED)
				s=s+"authenticated";

			s=s+"\n";

			s=s+prefix+" Channels:\n";
			for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
			{
				s= s + I->second->getStatus(prefix+"  ") + "\n";
			}

			s=s+prefix+" Devices:\n";
			for (CdeviceMap::iterator I=deviceMap.begin(); I!=deviceMap.end(); I++)
			{
				s=s + I->second->getStatus(prefix+"  ")+"\n";
			}

			return (s);
		}

		~Cserver()
		{

		}

	};

	SYNAPSE_REGISTER(asterisk_GetStatus)
	{
		Cmsg out;
		out.event="asterisk_Status";
		out.dst=msg.src;
		out["status"]="";

		out["status"].str()+="Sessions:\n";
		for (CsessionMap::iterator I=sessionMap.begin(); I!=sessionMap.end(); I++)
		{
			stringstream id;
			id << I->first;
			out["status"].str()+=I->second->getStatus(" ")+"\n";
		}


		out["status"].str()+="Groups:\n";
		for (CgroupMap::iterator I=groupMap.begin(); I!=groupMap.end(); I++)
		{
			out["status"].str()+=I->second->getStatus(" ")+"\n";
		}

		out["status"].str()+="Servers:\n";
		for (CserverMap::iterator I=serverMap.begin(); I!=serverMap.end(); I++)
		{
			out["status"].str()+= I->second.getStatus(" ");
		}


		out.send();
	}
	
	SYNAPSE_REGISTER(timer_Ready)
	{
		Cmsg out;
		out.clear();
		out.event="timer_Set";
		out["seconds"]=1;
		out["repeat"]=-1;
		out["dst"]=dst;
		out["event"]="asterisk_SendChanges";
		out.send();
	}
	
	

	
	SYNAPSE_REGISTER(asterisk_SendChanges)
	{
		CserverMap::iterator I;
//		I->second.sendChanges();

		//let all servers send their changes 
		for (CserverMap::iterator I=serverMap.begin(); I!=serverMap.end(); I++)
		{
			I->second.sendChanges();
		}

		//save state db if its changed
		stateDb.save();

	
	}
	
	
	SYNAPSE_REGISTER(ami_Ready)
	{
		///if ami is ready, we are ready ;)
		Cmsg out;
		out.clear();
		out.event="core_Ready";
		out.send();

		out.clear();
		out.event="asterisk_Config";
		out.send();
	}
	
	/** Connects to specified asterisk server
		\param host Hostname of the asterisk server.
		\param port AMI port (normally 5038)
		\param username Asterisk manager username
		\param password Asterisk manager password
	
	*/

// replaced by asterisk_Config for now, which lo
//	SYNAPSE_REGISTER(asterisk_Connect)
//	{
//		//ami connections are src-session based, so we need a new session for every connection.
//		Cmsg out;
//		out.event="core_NewSession";
//		out["server"]["username"]=msg["username"];
//		out["server"]["password"]=msg["password"];
//		out["server"]["port"]=msg["port"];
//		out["server"]["host"]=msg["host"];
//		out.send();
//	}

	/** Loads asterisk.conf and connects to specified servers.
	 * It will also handle server deletes/changes.
	 */
	SYNAPSE_REGISTER(asterisk_Config)
	{
		using synapse::Cconfig;
		Cconfig config;
		config.load("etc/synapse/asterisk.conf");

		//delete servers that have been changed or removed from the config
		for (CserverMap::iterator serverI=serverMap.begin(); serverI!=serverMap.end(); serverI++)
		{
			//server config removed or changed?
			if (
				!config.isSet(serverI->second.id) ||
				serverI->second.username!=config[serverI->second.id]["username"].str() ||
				serverI->second.password!=config[serverI->second.id]["password"].str() ||
				serverI->second.host!=config[serverI->second.id]["host"].str() ||
				serverI->second.port!=config[serverI->second.id]["port"].str()
			)
			{
				//delete server and connection by ending session
				//the rest will get cleaned up automaticly by the module_SessionEnd(ed) events.
				Cmsg out;
				out.src=serverI->first;
				out.event="core_DelSession";
				out.send();
			}
			else
			{
				//nothing changed, deleted it from config-object
				config.map().erase(serverI->second.id);
			}
		}

		//the remaining config entries need to be (re)created and (re)connected.
		for (Cconfig::iterator configI=config.begin(); configI!=config.end(); configI++)
		{
			//start a new session for every new connection, and supply the config info
			Cmsg out;
			out.event="core_NewSession";
			out["server"]=configI->second;
			out["server"]["id"]=configI->first;
			out["description"]="asterisk-ami connection session.";
			out.send();
		}
	}
	

	SYNAPSE_REGISTER(module_SessionStart)
	{
		//new session that is started with server info
		if (msg.isSet("server"))
		{
			//setup a new server object
			serverMap[msg.dst].id=msg["server"]["id"].str();
			serverMap[msg.dst].username=msg["server"]["username"].str();
			serverMap[msg.dst].password=msg["server"]["password"].str();
			serverMap[msg.dst].host=msg["server"]["host"].str();
			serverMap[msg.dst].port=msg["server"]["port"].str();
	
			//instruct ami to connect to the server
			Cmsg out;
			out.clear();
			out.event="ami_Connect";
			out.src=msg.dst;
			out["host"]=msg["server"]["host"].str();
			out["port"]=msg["server"]["port"].str();
			out.send();
		}
	}

	//a session owned by us ended, remove the corresponding server.
	SYNAPSE_REGISTER(module_SessionEnd)
	{
		//since the ami module also sees a module_SessionEnded, it will automaticly disconnect the server
		serverMap[msg.dst].clear();
		serverMap.erase(msg.dst);
	}

	//a session owned by another module ended. check if we created local stuff that we need to delete.
	SYNAPSE_REGISTER(module_SessionEnded)
	{
		if (sessionMap.find(msg["session"]) != sessionMap.end())
		{
			//remove the session. we use smartpointers , so it should be safe.
			DEB("Removing session object " << msg["session"]);
			sessionMap.erase(msg["session"]);
		}
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

		serverMap[msg.dst].status=Cserver::AUTHENTICATING;
	
	}
	
	SYNAPSE_REGISTER(ami_Response_Success)
	{
		
		//AMI is broken by design: why didnt they just use a Event, instead of defining the format of a Response exception. Now we need to use the ActionID and do extra marshhalling:
		//SIPshowPeer response
		if (msg["ActionID"].str()=="SIPshowPeer")
		{
			string deviceId=msg["Channeltype"].str()+"/"+msg["ObjectName"].str();
			CdevicePtr devicePtr=serverMap[msg.dst].getDevicePtr(deviceId);
	
			//NOTE: we handle Unmonitored sip peers as online, while we dont actually know if its online or not.		
			if (msg["Status"].str().find("OK")==0 || msg["Status"].str().find("Unmonitored")==0 )//||  msg["Status"].str().find("UNKNOWN")==0)
				devicePtr->setOnline(true);
			else
				devicePtr->setOnline(false);
	
			if (msg["Callerid"].str() != "\"\" <>")
			{
				//split up the the callerid+callerIdname string
				//Why the heck are the asterisk folks pre-formatting it like this?? Everywhere else in the ami a caller id is just a number instead of a preformatted string!
				smatch what;
				if (regex_search(
					msg["Callerid"].str(),
					what,
					boost::regex("\"(.*)\" <(.*)>")
				))
				{
					devicePtr->setCallerIdName(what[1]);
					devicePtr->setCallerId(what[2]);
				}
				else
				{
					//cant parse somehow? just set the whole string as callerIdName
					devicePtr->setCallerIdName(msg["CallerId"]);
				}
			}
			else
				devicePtr->setCallerIdName(msg["ObjectName"]);

			if (msg["ToHost"].str() != "")
				devicePtr->setTrunk(true);
			else
				devicePtr->setTrunk(false);


		}
		//login response
		else if (msg["ActionID"].str()=="Login")
		{
			serverMap[msg.dst].status=Cserver::AUTHENTICATED;
            Cmsg out;

			//learn all SIP peers as soon as we login
			out.clear();
			out.src=msg.dst;
			out.event="ami_Action";
			out["Action"]="SIPPeers";
			out.send();
	
            //learn all IAX peers as soon as we login
            out.clear();
            out.src=msg.dst;
            out.event="ami_Action";
            out["Action"]="IAXPeers";
            out.send();

            //learn all ZAP channels as soon as we login
            //NOTE: gives useless info
            // out.clear();
            // out.src=msg.dst;
            // out.event="ami_Action";
            // out["Action"]="ZapShowChannels";
            // out.send();

            //learn all DAHDI channels as soon as we login
            //NOTE: gives useless info
            // out.clear();
            // out.src=msg.dst;
            // out.event="ami_Action";
            // out["Action"]="DahdiShowChannels";
            // out.send();

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
		serverMap[msg.dst].status=Cserver::CONNECTING;
		
		//ami reconnects automaticly
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
	
	
	//we got a response to our SIPPeers/IAXpeers request.
    /* IAX2 1.8:
        |ChanObjectType = peer (string)
        |Channeltype = IAX2 (string)
        |Dynamic = no (string)
        |Encryption = no (string)
        |Event = PeerEntry (string)
        |IPaddress = 81.18.245.155 (string)
        |IPport = 4569 (string)
        |ObjectName = flexvoice/201-CDS_Datux (string)
        |Status = Unmonitored (string)
        |Trunk = yes (string)

       SIP 1.8:
        |ACL = no (string)
        |ChanObjectType = peer (string)
        |Channeltype = SIP (string)
        |Dynamic = yes (string)
        |Event = PeerEntry (string)
        |Forcerport = yes (string)
        |IPaddress = -none- (string)
        |IPport = 0 (string)
        |ObjectName = 1002 (string)
        |RealtimeDevice = no (string)
        |Status = Unmonitored (string)
        |TextSupport = no (string)
        |VideoSupport = no (string)

    */
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
	
    /*
        1.8:
        |Alarm = No Alarm (string)
        |Context = phones (string)
        |DAHDIChannel = 1 (string)
        |DND = Disabled (string)
        |Event = DAHDIShowChannels (string)
        |Signalling = FXO Kewlstart (string)
        |SignallingCode = 4128 (string)
    */
    SYNAPSE_REGISTER(ami_Event_DAHDIShowChannels)
    {

    }

	void channelStatus(Cmsg & msg)
	{
		CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Uniqueid"]);
		CdevicePtr devicePtr=serverMap[msg.dst].getDevicePtr(
			getDeviceIdFromChannel(msg["Channel"])
		);
	
		channelPtr->setDevicePtr(devicePtr);

		//States:
		// ringing: somebody is calling the device
		// in     : incoming call in progress
		// out    : outgoing call in progress
		// ''     : other (uninteresting) states

		//device is ringing
		if (msg["State"].str()=="Ringing")
		{
			channelPtr->setState("ringing");
		}
		//device is dialing (ringing other side)
		else if (msg["State"].str()=="Ring")
		{
			channelPtr->setState("out");
		}
		//device is connected with other side
		else if (msg["State"].str()=="Up")
		{
			if (channelPtr->getState()=="ringing")
			{
				channelPtr->setState("in");
			}
			else
			{
				channelPtr->setState("out");
			}
		}
		//device is down/other stuff
		else
		{
			channelPtr->setState("");
		}
	
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
	
//		devicePtr->sendChanges();
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
		Uniqueid: 1269871368.144

        1.8 dahdi horn pickup:
        |Account =  (string)
        |CallerIDName = <unknown> (string)
        |CallerIDNum = <unknown> (string)
        |Channel = DAHDI/1-1 (string)
        |ConnectedLineName = <unknown> (string)
        |ConnectedLineNum = <unknown> (string)
        |Event = Status (string)
        |Privilege = Call (string)
        |State = Rsrvd (string)
        |Uniqueid = 1352387005.54 (string)


        */
	
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
/*
        1.8 dahdi:	
        |Account =  (string)
        |CallerIDName = <unknown> (string)
        |CallerIDNum = <unknown> (string)
        |Channel = DAHDI/1-1 (string)
        |ConnectedLineName = <unknown> (string)
        |ConnectedLineNum = <unknown> (string)
        |Event = Status (string)
        |Privilege = Call (string)
        |State = Rsrvd (string)
        |Uniqueid = 1352387096.56 (string)
	
		channelStatus(msg);
*/	
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
	
//		devicePtr->sendChanges();
	
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
	
		channelPtr1->setLinkPtr(channelPtr2);
		channelPtr2->setLinkPtr(channelPtr1);
	
	
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
		channelPtr->sendDebug(msg, msg.dst);
	
		channelPtr=serverMap[msg.dst].getChannelPtr(msg["Uniqueid2"]);
		channelPtr->delLink();
		channelPtr->sendDebug(msg, msg.dst);
	
	
	
	}
	

	/** Requests authentication for \c src 
	
	\par Replys \c asterisk_authCall
		The session should authenticate by dialing a special crafted  number. After this asterisk knows to which device the client belongs, and can determine to which group the user belongs.
			\arg \c number The number the client should call to authenticate.

	\par Replys \c asterisk_authOk
		After the user called the specified number.
			\arg \c deviceId The deviceId the user was calling from.
	
	*/
	SYNAPSE_REGISTER(asterisk_authReq)
	{
		//create a Csession object for the src session?
		if (sessionMap.find(msg.src) == sessionMap.end())
		{
			sessionMap[msg.src]=CsessionPtr(new Csession(msg.src));
		}

		//deviceId + correct authCookie ?
		if (msg.isSet("deviceId") && msg.isSet("authCookie"))
		{
			//traverse all the servers and find the device
			for (CserverMap::iterator I=serverMap.begin(); I!=serverMap.end(); I++)
			{
				CdevicePtr devicePtr=I->second.getDevicePtr(msg["deviceId"],false);
				//device exists?
				if (devicePtr!=NULL)
				{
					//authCookie checks out?
					if (devicePtr->getAuthCookie()==msg["authCookie"])
					{

						//session is now authenticated, set corresponding group
						sessionMap[msg.src]->setGroupPtr(devicePtr->getGroupPtr());

						//session is re-authenticated, by authCookie
						Cmsg out;
						out.event="asterisk_authOk";
						out.dst=msg.src;
						out["deviceId"]=msg["deviceId"].str();
						out["authCookie"]=msg["authCookie"].str();
						out.send();
						return;
					}
				}
			}
		}
		else
		{
			//de-authenticate the user (e.g. logout)
			sessionMap[msg.src]->setGroupPtr(CgroupPtr());
		}

		//tell the client which number to call, to authenticate
		stringstream number;
		number << ASTERISK_AUTH << msg.src;

		Cmsg out;
		out.event="asterisk_reset"; //since we're logged out again, make sure the screen is cleared.
		out.dst=msg.src;
		out.send();

		out.event="asterisk_authCall";
		out["number"]=number.str();
		out.send();
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
	
		//somebody dialed the special authnumber?
		if (msg["Extension"].str().find(ASTERISK_AUTH)==0)
		{
			//determine specified session number
			int sessionId=lexical_cast<int>(msg["Extension"].str().substr(strlen(ASTERISK_AUTH)));
			//do we know the specified session?
			if (sessionMap.find(sessionId) != sessionMap.end())
			{
				//session is not yet authenticated?
				if (sessionMap[sessionId]->getGroupPtr() == NULL)
				{
					//get the device
					CdevicePtr devicePtr=serverMap[msg.dst].getDevicePtr(getDeviceIdFromChannel(msg["Channel"]));

					//session is now authenticated, set corresponding group
					sessionMap[sessionId]->setGroupPtr(devicePtr->getGroupPtr());

					Cmsg out;
					out.event="asterisk_authOk";
					out.dst=sessionId;
					out["deviceId"]=getDeviceIdFromChannel(msg["Channel"]);
					out["authCookie"]=serverMap[msg.dst].getDevicePtr(out["deviceId"])->getAuthCookie();
					out.send();
	
					//hang up
					out.clear();
					out.dst=msg.src;
					out.src=msg.dst;
					out.event="ami_Action";
					out["Action"]="Hangup";
					out["Channel"]=msg["Channel"].str();
					out.send();
	
					return;
				}
			}
		}
	
		CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Uniqueid"]);
		if (channelPtr->getFirstExtension()=="")
		{
			channelPtr->setFirstExtension(msg["Extension"]);
		}
	
		//TOO much info..channelPtr->sendDebug(msg, msg.dst);	
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
		channelPtr1->setLinkPtr(channelPtr2);
		channelPtr2->setLinkPtr(channelPtr1);
	
/*		channelPtr1->setInitiator(true);
		channelPtr2->setInitiator(false);*/
		
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
		if (channelPtr->getState()=="ringing")
		{
			channelPtr->setState("in");
		}
		else
		{
			channelPtr->setState("out");
		}
		channelPtr->setDevicePtr(devicePtr);
	
		if (channelPtr->getLinkPtr()==CchannelPtr())
		{
			//when we get renamed while NOT linked, we store the current callerids in the linkedcallerids.
			//after the rename we usually receive our new caller id immeadiatly
			channelPtr->setLinkCallerId(channelPtr->getCallerId());
			channelPtr->setLinkCallerIdName(channelPtr->getCallerIdName());
		}
	
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
		
		channelPtr->sendDebug(msg, msg.dst);
	
	}
	
	
	// 
	// 
	// SYNAPSE_REGISTER(ami_Event_PeerStatus)
	// {
	
	// 
	// }
	
}

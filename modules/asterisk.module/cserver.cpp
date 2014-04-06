
#include "cserver.h"
#include "cdevice.h"
#include "cchannel.h"
#include "cgroup.h"
#include "csession.h"
#include <boost/regex.hpp>


namespace asterisk
{
	//////////////////////////////////////////////////////////
	Cserver::Cserver(int sessionId, CserverMan * serverManPtr)
	{
		status=CONNECTING;
		this->sessionId=sessionId;
		this->serverManPtr=serverManPtr;
	}


	CdevicePtr Cserver::getDevicePtr(string deviceId, bool autoCreate)
	{
		if (deviceId=="")
			throw(synapse::runtime_error("Specified empty deviceId"));

		if (deviceMap.find(deviceId)==deviceMap.end())
		{
			if (autoCreate)
			{
				deviceMap[deviceId]=CdevicePtr(new Cdevice());
				deviceMap[deviceId]->setId(deviceId);

				//determine the group
				string groupId=this->group_default;
				smatch what;
				if (this->group_regex!="" && regex_search(
					deviceId,
					what, 
					boost::regex(this->group_regex)
				))
				{
					groupId=what[1];
				}

				if (serverManPtr->groupMap.find(groupId) == serverManPtr->groupMap.end())
				{
					//create new group
					serverManPtr->groupMap[groupId]=CgroupPtr(new Cgroup(serverManPtr));
					serverManPtr->groupMap[groupId]->setId(groupId);
				}

				deviceMap[deviceId]->setGroupPtr(serverManPtr->groupMap[groupId]);

				DEB("created device " << deviceId);
			}
			else
			{
				throw(synapse::runtime_error("deviceId not found"));
			}
		}
		return (deviceMap[deviceId]);
	}
	
	void Cserver::sendRefresh(int dst)
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

	void Cserver::sendChanges()
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

	CchannelPtr Cserver::getChannelPtr(string channelId, bool autoCreate)
	{
		if (channelId=="")
			throw(synapse::runtime_error("Specified empty channelId"));

		if (channelMap[channelId]==NULL)
		{
			if (autoCreate)
			{
				channelMap[channelId]=CchannelPtr(new Cchannel());
				channelMap[channelId]->setId(channelId);
				DEB("created channel " << channelId);
			}
			else
			{
				throw(synapse::runtime_error("channelId not found"));
			}
		}
		return (channelMap[channelId]);
	}

	void Cserver::delChannel(string channelId)
	{
		CchannelPtr channelPtr=getChannelPtr(channelId);
		channelPtr->del();
		channelMap.erase(channelId);
		DEB("deleted channel from map: " << channelId);

	}


	//on a disconnect this is called to remove all channels/devices.
	void Cserver::clear()
	{
		for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
		{
			//unlink all channels to prevent dangling shared_ptrs
			I->second->delLink();
			I->second->del();
		}


		//delete all chans 
		for (CdeviceMap::iterator I=deviceMap.begin(); I!=deviceMap.end(); I++)
			I->second->del();


		deviceMap.clear();
		channelMap.clear();
	}


	string Cserver::getStatus(string prefix)
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

	int Cserver::getSessionId()
	{
		return(sessionId);
	}


	void Cserver::amiCall(CdevicePtr fromDevice, CchannelPtr reuseChannelPtr, string exten)
	{
		
		Cmsg out;
		out.src=sessionId;
		out.event="ami_Action";


		//reuse channel by doing a redirect
		if (reuseChannelPtr!=CchannelPtr())
		{
			if (reuseChannelPtr->getDevicePtr()!=fromDevice)
				throw(synapse::runtime_error("specified channel does not belong to this device"));

			out["Action"]="Redirect";
			out["Channel"]=reuseChannelPtr->getChannelName();
			out["Context"]="from-internal";
			out["Priority"]=1;
			out["Exten"]=exten;


			CchannelPtr linkedChannelPtr=reuseChannelPtr->getLinkPtr();

			//"park" the linked channel
			if (linkedChannelPtr!=CchannelPtr())
			{
				out["ExtraChannel"]=linkedChannelPtr->getChannelName();
				out["ExtraContext"]="from-synapse";
				out["ExtraPriority"]=1;
				out["ExtraExten"]="901";
			}

			out.send();

		}
		//create new channel by doing a originate
		else
		{
			out["Action"]="Originate";
			out["Context"]="from-internal";
			out["Priority"]=1;
			out["Exten"]=exten;
			out["Channel"]=fromDevice->getId();
			out["Callerid"]="Calling "+exten;
			out.send();
		}
	}





	////////////////////////////////////////////////////////////////////////
	//server manager
	////////////////////////////////////////////////////////////////////////
	CserverMan::CserverMan(string stateFileName)
	{
		//load state database
		stateDb.load(stateFileName);

		//initialize randomizer
		//FIXME: unsafe randomiser?
		srand48_r(time(NULL), &randomBuffer);

	}

	void CserverMan::reload(string filename)
	{
		synapse::Cconfig config;
		config.load(filename);

		//delete servers that have been changed or removed from the config
		for (CserverMap::iterator serverI=serverMap.begin(); serverI!=serverMap.end(); serverI++)
		{
			//server config removed or changed?
			if (
				!config.isSet(serverI->second->id) ||
				serverI->second->username!=config[serverI->second->id]["username"].str() ||
				serverI->second->password!=config[serverI->second->id]["password"].str() ||
				serverI->second->host!=config[serverI->second->id]["host"].str() ||
				serverI->second->port!=config[serverI->second->id]["port"].str() ||
				serverI->second->host!=config[serverI->second->id]["group_default"].str() ||
				serverI->second->port!=config[serverI->second->id]["group_regex"].str()
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
				config.map().erase(serverI->second->id);
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

	//get server by session id
	CserverPtr CserverMan::getServerPtr(int sessionId)
	{
		if (serverMap.find(sessionId)==serverMap.end())
		{
			//create new
			DEB("Creating server object" << sessionId);
			serverMap[sessionId]=CserverPtr(new Cserver(sessionId, this));
		}
		return (serverMap[sessionId]);
	}

	//get server by server id
	CserverPtr CserverMan::getServerPtrByName(string id)
	{
		for (CserverMap::iterator I=serverMap.begin(); I!=serverMap.end(); I++)
		{
			if (I->second->id == id)
				return (I->second);
		}
		throw(synapse::runtime_error("server-name not found"));
	}

	void CserverMan::delServer(int sessionId)
	{
		if (serverMap.find(sessionId) != serverMap.end())
		{
			DEB("Removing server object " << sessionId);
			serverMap[sessionId]->clear();
			serverMap.erase(sessionId);
		}
	}

	TauthCookie CserverMan::getAuthCookie(string serverId, string deviceId)
	{
		//create new cookie?
		if (!stateDb[serverId][deviceId]["authCookie"])
		{
			TauthCookie authCookie;
			mrand48_r(&randomBuffer, &authCookie);
			stateDb[serverId][deviceId]["authCookie"]=authCookie;
			stateDb.changed();
			stateDb.save();
		}
			
		return(stateDb[serverId][deviceId]["authCookie"]);
	}


	CsessionPtr CserverMan::getSessionPtr(int id)
	{
		if (sessionMap.find(id)==sessionMap.end())
		{
			//create new
			DEB("Creating session object" << id);
			sessionMap[id]=CsessionPtr(new Csession(id));
		}
		return (sessionMap[id]);
	}

	bool CserverMan::sessionExists(int id)
	{
		return(sessionMap.find(id)!=sessionMap.end());
	}

	void CserverMan::delSession(int id)
	{
		if (sessionMap.find(id) != sessionMap.end())
		{
			//remove the session. we use smartpointers , so it should be safe.
			DEB("Removing session object " << id);
			sessionMap.erase(id);
		}
	}

	void CserverMan::send(string groupId, Cmsg & msg)
	{
		//broadcast?
		if (msg.dst==0)
		{
			//we cant simply broadcast it, we need to check group membership session by session
			for (CsessionMap::iterator I=sessionMap.begin(); I!=sessionMap.end(); I++)
			{
				if (I->second->isAdmin() || 
					( (I->second->getDevicePtr()!=NULL) && I->second->getDevicePtr()->getGroupPtr()->getId()==groupId )
					)
				{
					//send it to that session only
					msg.dst=I->first;
					try
					{
						msg.send();
					}
					catch(const std::exception& e)
					{
						WARNING("asterisk: delivering broadcast to " << msg.dst << " failed: " << e.what());
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
				if (I->second->isAdmin() || 
					( (I->second->getDevicePtr()!=NULL) && I->second->getDevicePtr()->getGroupPtr()->getId()==groupId )
					)
				{
					try
					{
						msg.send();
					}
					catch(const std::exception& e)
					{
						WARNING("asterisk: send to " << msg.dst << " failed: " << e.what());
					}
				}
			}
		}
	}

	string CserverMan::getStatus()
	{
		string ret;
		ret+="Sessions:\n";
		for (CsessionMap::iterator I=sessionMap.begin(); I!=sessionMap.end(); I++)
		{
			stringstream id;
			id << I->first;
			ret+=I->second->getStatus(" ")+"\n";
		}


		ret+="Groups:\n";
		for (CgroupMap::iterator I=groupMap.begin(); I!=groupMap.end(); I++)
		{
			ret+=I->second->getStatus(" ")+"\n";
		}

		ret+="Servers:\n";
		for (CserverMap::iterator I=serverMap.begin(); I!=serverMap.end(); I++)
		{
			ret+= I->second->getStatus(" ");
		}

		return(ret);

	}

	void CserverMan::sendChanges()
	{
		//let all servers send their changes 
		for (CserverMap::iterator I=serverMap.begin(); I!=serverMap.end(); I++)
		{
			I->second->sendChanges();
		}		
	}

	void CserverMan::sendRefresh(int dst)
	{
		//indicate start of a refresh, deletes all known state-info in client
		Cmsg out;
		out.event="asterisk_reset";
		out.dst=dst; 
		out.send();
	
		for (CserverMap::iterator I=serverMap.begin(); I!=serverMap.end(); I++)
		{
			I->second->sendRefresh(dst);
		}
	}

}

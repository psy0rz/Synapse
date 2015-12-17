
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

		if (channelMap.find(channelId)==channelMap.end())
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



	void Cserver::amiSetVar(CchannelPtr channelPtr, string variable, string value)
	{
		Cmsg out;
		out.src=sessionId;
		out.event="ami_Action";
		out["Action"]="Setvar";
		out["Channel"]=channelPtr->getChannelName();
		out["Variable"]=variable;
		out["Value"]=value;
		out.send();
	}

	void Cserver::amiUpdateCallerIdAll(CchannelPtr channelPtr, string all)
	{
		amiSetVar(channelPtr, "CONNECTEDLINE(all)", all);
	}


	void Cserver::amiUpdateCallerIdName(CchannelPtr channelPtr, string name)
	{
		amiSetVar(channelPtr, "CONNECTEDLINE(name)", name);
	}

	void Cserver::amiUpdateCallerId(CchannelPtr channelPtr, string num)
	{
		amiSetVar(channelPtr, "CONNECTEDLINE(num)", num);
	}



	void Cserver::amiRedirect(CchannelPtr channel1Ptr, string context1, string exten1, CchannelPtr channel2Ptr, string context2, string exten2)
	{
		Cmsg out;
		out.src=sessionId;
		out.event="ami_Action";

		out["Action"]="Redirect";

		out["Channel"]=channel1Ptr->getChannelName();
		out["Context"]=context1;
		out["Priority"]=1;
		out["Exten"]=exten1;

		if (channel2Ptr!=CchannelPtr())
		{
			out["ExtraChannel"]=channel2Ptr->getChannelName();
			out["ExtraContext"]=context2;
			out["ExtraPriority"]=1;
			out["ExtraExten"]=exten2;
		}
		out.send();
	}

	void Cserver::amiHangup(CchannelPtr channelPtr)
	{
		Cmsg out;
		out.src=sessionId;
		out.event="ami_Action";
		out["Action"]="Hangup";
		out["Channel"]=channelPtr->getChannelName();
		out.send();
	}

	void Cserver::amiCall(CdevicePtr fromDevicePtr, CchannelPtr reuseChannelPtr, string exten)
	{
		

		if (fromDevicePtr==CdevicePtr())
			throw(synapse::runtime_error("device not specified"));

		//reuse channel by doing a redirect
		if (reuseChannelPtr!=CchannelPtr())
		{
			if (reuseChannelPtr->getDevicePtr()!=fromDevicePtr)
				throw(synapse::runtime_error("specified channel does not belong to this device"));

			//park the other channel and make the call
			CchannelPtr linkedChannelPtr=reuseChannelPtr->getLinkPtr();

			if (linkedChannelPtr!=CchannelPtr())
				amiSetVar(linkedChannelPtr, "__SYNAPSE_OWNER", fromDevicePtr->getId());

			amiRedirect(reuseChannelPtr, "from-internal", exten, 
						linkedChannelPtr, "from-synapse", "park");
		}
		//create new channel by doing a originate
		else
		{
			Cmsg out;
			out.src=sessionId;
			out.event="ami_Action";

			out["Action"]="Originate";
			out["Context"]="from-internal";
			out["Priority"]=1;
			out["Exten"]=exten;
			out["Channel"]=fromDevicePtr->getId();
			out["Callerid"]=fromDevicePtr->getCallerIdAll();
			out["Async"]="true";
			out.send();
		}
	}


	// void Cserver::amiPreparePark(CchannelPtr channelPtr)
	// {
	// 	amiUpdateCallerIdName(channelPtr, "[parked] "+channelPtr->getCallerIdName());
	// 	amiSetVar(channelPtr, "__SYNAPSE_OWNER", channelPtr->getDevicePtr()->getId());
	// }


	// void Cserver::amiPark(CchannelPtr channel1Ptr, CchannelPtr channel2Ptr)
	// {
	// 	amiPreparePark(channel1Ptr);
	// 	if (channel2Ptr!=CchannelPtr())
	// 		amiPreparePark(channel2Ptr);


	// 	amiRedirect(channel1Ptr, "from-synapse", "901",
	// 			channel2Ptr, "from-synapse", "901");

	// }

	//park channel, hangup the linked channel
	void Cserver::amiPark(CdevicePtr fromDevicePtr, CchannelPtr channel1Ptr)
	{

		if (fromDevicePtr==CdevicePtr())
			throw(synapse::runtime_error("device not specified"));

		if (channel1Ptr==CchannelPtr())
			throw(synapse::runtime_error("channel1 not specified"));

		if (channel1Ptr->getDevicePtr()!=fromDevicePtr && (channel1Ptr->getLinkPtr()==CchannelPtr() || channel1Ptr->getLinkPtr()->getDevicePtr()!=fromDevicePtr))
				throw(synapse::runtime_error("specified channel1 does not belong to this device"));

		amiSetVar(channel1Ptr, "__SYNAPSE_OWNER", fromDevicePtr->getId());
		amiUpdateCallerIdAll(channel1Ptr, "[parked] "+fromDevicePtr->getCallerIdAll());

		if (channel1Ptr->getLinkPtr()==CchannelPtr())
		{
			amiRedirect(channel1Ptr, "from-synapse", "park");
		}
		else
		{
			amiRedirect(channel1Ptr, "from-synapse", "park",
						channel1Ptr->getLinkPtr(), "from-synapse", "hangup");
		}
	}

	void Cserver::amiBridge(CdevicePtr fromDevicePtr, CchannelPtr channel1Ptr, CchannelPtr channel2Ptr, bool parkLinked1)
	{
		Cmsg out;
		out.src=sessionId;
		out.event="ami_Action";

		if (fromDevicePtr==CdevicePtr())
			throw(synapse::runtime_error("device not specified"));

		if (channel2Ptr==CchannelPtr())
			throw(synapse::runtime_error("channel2 not specified"));

		//bridge 2 existing channels
		if (channel1Ptr!=CchannelPtr())
		{
			if (channel1Ptr->getDevicePtr()!=fromDevicePtr && (channel1Ptr->getLinkPtr()==CchannelPtr() || channel1Ptr->getLinkPtr()->getDevicePtr()!=fromDevicePtr))
				throw(synapse::runtime_error("specified channel1 does not belong to this device"));

			// //park linkedChannel1?
			// if (parkLinked1 && channel1Ptr->getLinkPtr()!=CchannelPtr())
			// {
			// 	//we HAVE to park both the channel and the linkedChannel, otherwise we lose the call:
			// 	amiPark(channel1Ptr, channel1Ptr->getLinkPtr());
			// }

			// //park linkedChannel2?
			// if (parkLinked2 && channel2Ptr->getLinkPtr()!=CchannelPtr())
			// {
			// 	//we HAVE to park both the channel and the linkedChannel, otherwise we lose the call:
			// 	amiPark(channel2Ptr, channel2Ptr->getLinkPtr());
			// }

			//now bridge the channels together
			// out.clear();
			// out["Action"]="Bridge";
			// out["Channel1"]=channel1Ptr->getChannelName();
			// out["Channel2"]=channel2Ptr->getChannelName();
			// out["Tone"]="yes";
			// out.send();

			//park linkedChannel1?
			if (parkLinked1 && channel1Ptr->getLinkPtr()!=CchannelPtr())
			{

				//tell the channel1 linked channel its parked by the person at channel1
				amiUpdateCallerIdAll(channel1Ptr->getLinkPtr(), "[parked] "+fromDevicePtr->getCallerIdAll());

				//tell the channel1 its redirected to channel2
				amiUpdateCallerIdAll(channel1Ptr, channel2Ptr->getCallerIdAll());

				//tell the target channel its redirected to channel1.
				amiUpdateCallerIdName(channel2Ptr, channel1Ptr->getCallerIdAll());

				//park the linked channel and redirect this channel to channel2
				//anything linked to channel2 is dropped
				amiSetVar(channel1Ptr, "__SYNAPSE_BRIDGE", channel2Ptr->getChannelName());
				amiSetVar(channel1Ptr->getLinkPtr(), "__SYNAPSE_OWNER", fromDevicePtr->getId());
				amiRedirect(channel1Ptr, "from-synapse", "bridge",
							channel1Ptr->getLinkPtr(), "from-synapse", "park");
			}
			else
			{
				//just bridge it to the channel
				amiSetVar(channel1Ptr, "__SYNAPSE_BRIDGE", channel2Ptr->getChannelName());
				amiRedirect(channel1Ptr, "from-synapse", "bridge");
			}


		}
		//create new channel and bridge it to channel2
		else
		{
			out.clear();
			out["Action"]="Originate";
			out["Channel"]=fromDevicePtr->getId();
			out["Callerid"]=fromDevicePtr->getCallerIdAll();
			out["Context"]="from-synapse";
			out["Priority"]=1;
			out["Exten"]="bridge";
			out["Async"]="true";
			out["Variable"]="__SYNAPSE_BRIDGE="+channel2Ptr->getChannelName();
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
		for (synapse::Cconfig::iterator configI=config.begin(); configI!=config.end(); configI++)
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


namespace asterisk
{

	Cserver::Cserver(int sessionId)
	{
		status=CONNECTING;
		this->sessionId=sessionId;
	}


	CdevicePtr Cserver::getDevicePtr(string deviceId, bool autoCreate=true)
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

				if (groupMap.find(groupId) == groupMap.end())
				{
					//create new group
					groupMap[groupId]=CgroupPtr(new Cgroup());
					groupMap[groupId]->setId(groupId);
				}

				deviceMap[deviceId]->setGroupPtr(groupMap[groupId]);					

				DEB("created device " << deviceId);
			}
			else
			{
				return (CdevicePtr());
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

	CchannelPtr Cserver::getChannelPtr(string channelId)
	{
		if (channelId=="")
			throw(synapse::runtime_error("Specified empty channelId"));

		if (channelMap[channelId]==NULL)
		{
			channelMap[channelId]=CchannelPtr(new Cchannel());
			channelMap[channelId]->setId(channelId);
			DEB("created channel " << channelId);
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
		//unlink all channels to prevent dangling shared_ptrs
		for (CchannelMap::iterator I=channelMap.begin(); I!=channelMap.end(); I++)
		{
			I->second->delLink();
		}

		//this should send out automated delChannel/delDevice events, on destruction of the objects
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



	//server manager
	CserverMan::CserverMan(string  stateFileName);
	{
		//load state database
		stateDb.load(stateFileName);

		//initialize randomizer
		//FIXME: unsafe randomiser?
		srand48_r(time(NULL), &randomBuffer);

	}

	//get server by session id
	CserverPtr CserverMan::getServerPtr(int sessionId)
	{
		if (serverMap.find(sessionId==serverMap.end())
		{
			//create new
			DEB("Creating server object" << sessionId;
			serverMap[sessionId]=CserverPtr(new Cserver(sessionId));
		}
		return (serverMap[sessionId]);
	}

	//get server by server id
	CserverPtr CserverMan::getServerPtr(string id)
	{
		for (CserverMap::iterator I=serverMap.begin(); I!=serverMap.end(); I++)
		{
			if (I->second->id == id)
				return (I->second);
		}
		return(NULL);
	}

	void CserverMan::delServer(int sessionId)
	{
		if (serverMap.find(sessionId) != serverMap.end())
		{
			DEB("Removing server object " << sessionId);
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

	bool CserverMan::sessionExists(id)
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



}
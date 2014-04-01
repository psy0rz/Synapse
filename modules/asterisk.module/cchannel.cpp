namespace asterisk
{

	Cchannel::Cchannel()
	{
		changes=1;
		changesSent=0;
//			initiator=true;
	}

	bool Cchannel::sendDebug(Cmsg msg, int serverId)
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

	int Cchannel::getChanges()
	{
		return (changes);
	}


	void Cchannel::setFirstExtension(string firstExtension)
	{
		if (firstExtension!=this->firstExtension)
		{
			this->firstExtension=firstExtension;
			changes++;
		}

	}

	string Cchannel::getFirstExtension()
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

	void Cchannel::setDevicePtr(CdevicePtr devicePtr)
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

	void Cchannel::setId(string id)
	{
		if (id!=this->id)
		{
			this->id=id;
			changes++;
		}
	}

	void Cchannel::setLinkPtr(CchannelPtr channelPtr)
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

	void Cchannel::delLink()
	{
		//unset our link
		setLinkPtr(CchannelPtr());
	}

	CchannelPtr Cchannel::getLinkPtr()
	{
		return(linkChannelPtr);
	}


	void Cchannel::setCallerId(string callerId)
	{
		if (callerId!=this->callerId)
		{
			this->callerId=callerId;
			if (linkChannelPtr!=CchannelPtr())
				linkChannelPtr->setLinkCallerId(callerId);
			changes++;
		}
	}

	void Cchannel::setCallerIdName(string callerIdName)
	{
		if (callerIdName!=this->callerIdName)
		{
			this->callerIdName=callerIdName;
			if (linkChannelPtr!=CchannelPtr())
				linkChannelPtr->setLinkCallerIdName(callerIdName);
			changes++;
		}
	}

	void Cchannel::setLinkCallerId(string callerId)
	{
		if (callerId!=this->linkCallerId)
		{
			this->linkCallerId=callerId;
			changes++;
		}
	}

	void Cchannel::setLinkCallerIdName(string callerIdName)
	{
		if (callerIdName!=this->linkCallerIdName)
		{
			this->linkCallerIdName=callerIdName;
			changes++;
		}
	}


	string Cchannel::getCallerId()
	{
		return (callerId);
	}

	string Cchannel::getCallerIdName()
	{
		return (callerIdName);
	}

	string Cchannel::getState()
	{
		return (state);
	}

	void Cchannel::setState(string state)
	{

		if (state!=this->state)
		{
			this->state=state;
			changes++;
		}
	}

	void Cchannel::sendChanges()
	{

		// are there changes we didnt send yet?
		if (changes>changesSent)
		{
			if (sendUpdate())
				changesSent=changes;
		}	


	}

	bool Cchannel::sendUpdate(int forceDst)
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

	void Cchannel::sendRefresh(int dst)
	{
		sendUpdate(dst);
	}

	string Cchannel::getStatus(string prefix)
	{
		if (devicePtr!=NULL)
		{
			return (
				prefix + "Channel " + id + ":\n" +
				devicePtr->getStatus(prefix+" ")
			);
		}
		else
		{
			return (
				prefix + "Channel " + id + " (no devices)\n"
			);

		}
	}

	//delete channel
	void Cchannel::del()
	{
		//delete the link, to prevent "hanging" shared_ptrs
		delLink();

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
	};

}
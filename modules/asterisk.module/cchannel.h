#ifndef CCHANNEL_H_
#define CCHANNEL_H_

#include "typedefs.h"
#include <boost/shared_ptr.hpp>
#include "cmsg.h"

namespace asterisk
{
	using namespace synapse;

	//asterisk channels. these always point to a corresponding Cdevice
	//they can also point to a 'linked' channel. (when asterisk links two channels together)
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
		CdevicePtr ownerPtr;
		string channelName;

		int changesSent;		

		public:

		Cchannel();
		bool sendDebug(Cmsg msg, int serverId);
		int getChanges();
		void setFirstExtension(string firstExtension);
		string getFirstExtension();
		void setDevicePtr(CdevicePtr devicePtr);
		void setOwnerPtr(CdevicePtr devicePtr);  //"owner" of the channel, when doing parks (only owner can unpark)
		void setId(string id);
		void setLinkPtr(CchannelPtr channelPtr);
		void delLink();
		CchannelPtr getLinkPtr();
		void setCallerId(string callerId);
		void setCallerIdName(string callerIdName);
		void setLinkCallerId(string callerId);
		void setLinkCallerIdName(string callerIdName);
		string getCallerId();
		string getCallerIdName();
		string getState();
		void setChannelName(string channelName);
		string getChannelName();
		void setState(string state);
		void sendChanges();
		bool sendUpdate(int forceDst=0);
		void sendRefresh(int dst);
		string getStatus(string prefix);
		void del();

	};

	
}

#endif

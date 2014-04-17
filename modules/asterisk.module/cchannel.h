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

		//the channel uniqueId, e.g.: 1396987163.599
		string id; 
		//the channel name, e.g.: SIP/100-00000053
		string channelName;
		//NOTE: the channelName of a 'real' channel can sometimes change temporary. for exampling when bridging it can become SIP/100-00000053<MASQ>. 
		//To circumvent this, the asterisk developers created the uniqueId: this will stay the same for a channel.
		//Altough for many simple things channels are deleted and recreated as well, making things even more difficult.
		//All in all asterisk seems like a big kludge...its like the Sendmail of opensource PBX-es: it has all the features but is one big mess. 
		//When it has a good GUI that suports all the features, i'm sure we will migrate away from asterisk to Freeswitch.


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

		int changesSent;
		bool onHold;

		public:

		Cchannel();
		bool sendDebug(Cmsg msg, int serverId);
		int getChanges();
		void setFirstExtension(string firstExtension);
		string getFirstExtension();
		void setDevicePtr(CdevicePtr devicePtr);
		CdevicePtr getDevicePtr();
		void setOwnerPtr(CdevicePtr devicePtr);  //"owner" of the channel, when doing parks (only owner can unpark)
		void setId(string id);
		string getId();
		void setLinkPtr(CchannelPtr channelPtr);
		void delLink();
		CchannelPtr getLinkPtr();
		void setCallerId(string callerId);
		void setCallerIdName(string callerIdName);
		void setLinkCallerId(string callerId);
		string getLinkCallerId();
		void setLinkCallerIdName(string callerIdName);
		string getLinkCallerIdName();
		string getCallerId();
		string getCallerIdName();
		string getCallerIdAll(); //name <number>
		string getState();
		void setChannelName(string channelName);
		string getChannelName();
		void setState(string state);
		void sendChanges();
		bool sendUpdate(int forceDst=0);
		void sendRefresh(int dst);
		string getStatus(string prefix);
		void del();
		void setOnHold(bool setOnHold);
		bool getOnHold();


	};

	
}

#endif

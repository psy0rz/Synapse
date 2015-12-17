#ifndef CSERVER_H_
#define CSERVER_H_


/*
    Data structure overview:

	serverMan
		groupMap
			groupPtr
				group
					serverManPtr
						...

		serverMap
			serverPtr
				server
					deviceMap
						devicePtr
							device
								groupPtr
									...
					channelMap
						channelPtr
							channel
								devicePtr
									...

		sessionMap
			sessionPtr
				session
					devicePtr
						...
					serverPtr
						...


*/

#include "typedefs.h"
#include "cconfig.h"
#include "cmsg.h"

namespace asterisk
{

	class CserverMan;


	//physical asterisk servers. every server has its own device and channel map
	class Cserver
	{
		private:
		CchannelMap channelMap;
		CdeviceMap deviceMap;




		CserverMan * serverManPtr;

		int sessionId; //synapse sessionID of the ami-connection.

		public:
		string id;
		string username;
		string password;
		string host;
		string port;
		string group_regex;
		string group_default;

		enum Estatus
		{
			CONNECTING,
			AUTHENTICATING,
			AUTHENTICATED,
		};
		Estatus status;

		Cserver(int sessionId, CserverMan * serverManPtr);
		CdevicePtr getDevicePtr(string deviceId, bool autoCreate=true);
		void sendRefresh(int dst);
		void sendChanges();
		CchannelPtr getChannelPtr(string channelId, bool autoCreate=true);
		void delChannel(string channelId);
		void clear();
		string getStatus(string prefix);
		int getSessionId();

		//sets a asterisk variable on a channel
		void amiSetVar(CchannelPtr channelPtr, string variable, string value);

		//redirect one or two channels to another context and extention. 
		void amiRedirect(CchannelPtr channel1Ptr, string context1, string exten1, CchannelPtr channel2Ptr=CchannelPtr(), string context2="", string exten2="");

		//update the caller id or number of a channel in realtime. 
		//you need to enable sendrpid on the extension and have a supported phone (like a cisco spa)
		void amiUpdateCallerIdName(CchannelPtr channelPtr, string name);
		void amiUpdateCallerId(CchannelPtr channelPtr, string num);
		void amiUpdateCallerIdAll(CchannelPtr channelPtr, string all);


		void amiPark(CdevicePtr fromDevicePtr, CchannelPtr channel1Ptr, string mode1, CchannelPtr channel2Ptr, string mode2);


		//Make a call from specified device to extention
		//Tries to be reuse the specified channel (parking the otherside of the call)
		//Otherwise originates a new call on device.
		void amiCall(CdevicePtr fromDevicePtr, CchannelPtr reuseChannelPtr, string exten);

		//Bridge 2 channels together.
		//If channelA is not specified, it will originate a new call and then do the bridging to channelB via the bridge app.
		//set parkLinked to true if you want to park any linked channels. (that are currently bridged)
		void amiBridge(CdevicePtr fromDevicePtr, CchannelPtr channel1Ptr, CchannelPtr channel2Ptr, bool parkLinked1);

		//hangup the channel
		void amiHangup(CchannelPtr channelPtr);

	};


	//main object that contains all the others (sessions, devices, channels ,groups)
	class CserverMan
	{
		private:
		CserverMap serverMap;
		CsessionMap sessionMap;
	 	synapse::Cconfig stateDb;
	 	struct drand48_data randomBuffer;


		public:

		CgroupMap groupMap; //managed from Cserver.

		void reload(string filename);

		CserverMan(string stateFileName);
		CserverPtr getServerPtr(int sessionId); //server ami connection session
		CserverPtr getServerPtrByName(string id);
		void delServer(int sessionId);
		TauthCookie getAuthCookie(string serverId, string deviceId);

		//sends msg after applying filtering.
		//message will only be sended or broadcasted to sessions that belong to this group.
		//some channels like locals and trunks will also be filtered, depending on session-specific preferences
		void send(string groupId, Cmsg & msg);

		//user sessions (http)
		CsessionPtr getSessionPtr(int id);
		void delSession(int id);
		bool sessionExists(int id);


		string getStatus();
		void sendChanges();
		void sendRefresh(int dst);

	};
};

#endif

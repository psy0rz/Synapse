#ifndef CSERVER_H_
#define CSERVER_H_


/*
    Data structure overview:

	serverMan
		groupMap
			groupPtr
				group

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

#include "./cdevice.h"
#include "./cchannel.h"
#include "./cconfig.h"
#include "./cgroup.h"
#include "./csession.h"

namespace asterisk
{


	typedef shared_ptr<class Cserver> CserverPtr;

	//physical asterisk servers. every server has its own device and channel map
	class Cserver
	{
		private:
		typedef map<string, CchannelPtr> CchannelMap;
		CchannelMap channelMap;

		typedef map<string, CdevicePtr> CdeviceMap;
		CdeviceMap deviceMap;

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
		string group_regex;
		string group_default;

		int sessionId; //synapse sessionID of the ami-connection.

		Cserver(int sessionId);
		CdevicePtr getDevicePtr(string deviceId, bool autoCreate=true);
		void sendRefresh(int dst);
		void sendChanges();
		CchannelPtr getChannelPtr(string channelId);
		void delChannel(string channelId);
		void clear();
		string getStatus(string prefix);

	};

	typedef long int TauthCookie;

	//main object that contains all the others (sessions, devices, channels ,groups)
	class CserverMan
	{
		private:
		typedef map<int, CserverPtr> CserverMap;
		CserverMap serverMap;

		typedef map<int, CsessionPtr> CsessionMap;
		CsessionMap sessionMap;

		typedef map<string, CgroupPtr> CgroupMap;
		CgroupMap groupMap;

	 	synapse::Cconfig stateDb;
	 	struct drand48_data randomBuffer;


		public:
		CserverMan(string stateFileName);
		CserverPtr getServerPtr(int sessionId); //server ami connection session
		CserverPtr getServerPtr(string id);
		void delServer(int sessionId);
		TauthCookie getAuthCookie(string serverId, string deviceId);

		//user sessions (http)
		CsessionPtr getSessionPtr(int id);
		void delSession(int id);
		bool sessionExists(int id);
	};
};

#endif

#ifndef CSERVER_H_
#define CSERVER_H_

#include "cdevice.h"
#include "cchannel.h"

namespace asterisk
{


	typedef shared_ptr<class Cchannel> CchannelPtr;
	typedef shared_ptr<class Cdevice> CdevicePtr;

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

		int sessionId;

		Cserver(int sessionId);
		CdevicePtr getDevicePtr(string deviceId, bool autoCreate=true);
		void sendRefresh(int dst);
		void sendChanges();
		CchannelPtr getChannelPtr(string channelId);
		void delChannel(string channelId);
		void clear();
		string getStatus(string prefix);

	};

	class CserverMan
	{
		private:
		typedef map<int, class Cserver> CserverMap;
		CserverMap serverMap;

		public:
		CserverPtr getServerPtr(int sessionId);
		void delServer(int sessionId);


	};

};

#endif

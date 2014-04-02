#ifndef CDEVICE_H_
#define CDEVICE_H_

#include "typedefs.h"
#include <boost/shared_ptr.hpp>
#include <string>
namespace asterisk
{

	using namespace std;


	
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
		string exten; //extension this device can be called on
		//TauthCookie authCookie;
	
		public:

		Cdevice();
		bool isFiltered();
		CgroupPtr getGroupPtr();
		void setId(string id);
		void setGroupPtr(CgroupPtr groupPtr);
		string getId();
		void setCallerId(string callerId);
		string getCallerId();
		void setCallerIdName(string callerIdName);
		string getCallerIdName();
		void setTrunk(bool trunk);
		void setOnline(bool online);
		void setExten(string exten);
		bool sendUpdate(int forceDst=0);
		void sendChanges();
		void sendRefresh(int dst);
		string getStatus(string prefix);
		~Cdevice();
		
	};


}

#endif

#ifndef CPAPERCLIENT
#define CPAPERCLIENT


#include "cclient.h"
#include "cvar.h"
#include "cmsg.h"

namespace paper
{
	using namespace std;

	//a client of a paper object
	class CpaperClient : public synapse::Cclient
	{
		friend class CpaperObject;
		private:

		public:
		Cvar mCursor;
		int mLastElementId;

		//arbitrary info field for stuff like name
		Cvar mInfo;

		//authorized functions
		bool mAuthCursor;
		bool mAuthChat;
		bool mAuthChange;
		bool mAuthOwner;
		string mAuthDescription;



		CpaperClient();


		//sends a message to the client, only if the client has view-rights
		//otherwise the message is ignored.
//		void sendFiltered(Cmsg & msg);

		//authorizes the client withclient with key and rights
		void authorize(Cvar & rights);

		virtual void getInfo(Cvar & var);
		void setInfo(Cvar & var);
	};
}
#endif

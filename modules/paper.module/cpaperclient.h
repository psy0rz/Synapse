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


		//authorized functions
		bool mAuthCursor;
		bool mAuthChat;
		bool mAuthView;
		bool mAuthChange;
		bool mAuthOwner;



		CpaperClient();


		//sends a message to the client, only if the client has view-rights
		//otherwise the message is ignored.
		void sendFiltered(Cmsg & msg);

		//authorizes the client withclient with key and rights
		void authorize(Cvar & rights);
	};
}
#endif

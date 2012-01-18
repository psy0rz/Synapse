#include "cpaperclient.h"

namespace paper
{

	CpaperClient::CpaperClient()
	{
		mLastElementId=0;
		mAuthView=false;
		mAuthChange=false;
		mAuthOwner=false;
		mAuthCursor=false;
		mAuthChat=false;


	}


	//sends a message to the client, only if the client has view-rights
	//otherwise the message is ignored.
	void CpaperClient::sendFiltered(Cmsg & msg)
	{
		if (mAuthView)
			msg.send();

	}

	//authorizes the client withclient with key and rights
	void CpaperClient::authorize(Cvar & rights)
	{
		mAuthView=rights["view"];
		mAuthChange=rights["change"];
		mAuthOwner=rights["owner"];
		mAuthCursor=rights["cursor"];
		mAuthChat=rights["chat"];

		//inform the client of its new rights
		Cmsg out;
		out.event="paper_Authorized";
		out.dst=id;
		out.map()=rights;
		out.send();

	}
}

#include "cpaperclient.h"

namespace paper
{

	CpaperClient::CpaperClient()
	{
		mLastElementId=0;
		mAuthChange=false;
		mAuthOwner=false;
		mAuthCursor=false;
		mAuthChat=false;


	}


	//sends a message to the client, only if the client has view-rights
	//otherwise the message is ignored.
//	void CpaperClient::sendFiltered(Cmsg & msg)
//	{
//		if (mAuthView)
//			msg.send();
//
//	}

	//authorizes the client withclient with key and rights
	void CpaperClient::authorize(Cvar & rights)
	{
		mAuthChange=rights["change"];
		mAuthOwner=rights["owner"];
		mAuthCursor=rights["cursor"];
		mAuthChat=rights["chat"];
		mAuthDescription=rights["description"].str();

//		//inform the client of its new rights
//		Cmsg out;
//		out.event="paper_Authorized";
//		out.dst=id;
//		out.map()=rights;
//		out.send();

	}

	/** Sets info fields of client (can be anything)
	 *
	 */
	void CpaperClient::setInfo(Cvar & var)
	{
		mInfo=var;
	}


	/** Fills var with information about the client.
	 * \P clientId Id of client
	 * \P rights.change Set to 1 when client has rights to change drawing.
	 * \P rights.owner Set to 1 when client  is owner.
	 * \P rights.cursor Set to 1 when client may show a cursor.
	 * \P rights.chat Set to 1 when client may chat.
	 * \P rights.description Description of the key
	 *
	 */
	void CpaperClient::getInfo(Cvar & var)
	{
		var=mInfo;
		var["clientId"]=id;
		var["rights"]["change"]=mAuthChange;
		var["rights"]["owner"]=mAuthOwner;
		var["rights"]["cursor"]=mAuthCursor;
		var["rights"]["chat"]=mAuthChat;
		var["rights"]["description"]=mAuthDescription;
	}

}

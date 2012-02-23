/*  Copyright 2008,2009,2010 Edwin Eefting (edwin@datux.nl) 

    This file is part of Synapse.

    Synapse is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Synapse is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Synapse.  If not, see <http://www.gnu.org/licenses/>. */


/** \file
Internet paper.

*/
#include "synapse.h"
#include <time.h>
#include <set>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "cconfig.h"
#include <boost/regex.hpp>
#include <stdlib.h>

#include "cpaperobject.h"
#include "cobjectman.h"

#include "exception/cexception.h"

/** Paper namespace
 *
 */
namespace paper
{

	using namespace std;


	bool gShutdown;
	//most recent changed paper list
	synapse::Cconfig gPaperIndex;

	SYNAPSE_REGISTER(module_Init)
	{

		Cmsg out;

		gShutdown=false;
		gPaperIndex.load("var/paper/index");

		//this module is single threaded
		out.clear();
		out.event="core_ChangeModule";
		out["maxThreads"]=1;
		out.send();

		out.clear();
		out.event="core_ChangeSession";
		out["maxThreads"]=1;
		out.send();

		//client send-only events:
		out.clear();
		out.event="core_ChangeEvent";
		out["modifyGroup"]=	"modules";
		out["sendGroup"]=	"anonymous";
		out["recvGroup"]=	"modules";
		out["event"]=	"paper_Create";		out.send(); //create new object
		out["event"]=	"paper_Check";		out.send(); //check if object exists/access is allowed
		out["event"]=	"paper_Delete";		out.send(); //delete a object
		out["event"]=	"paper_Login";		out.send(); //join a object
		out["event"]=	"paper_Leave";		out.send(); //leave a object
		out["event"]=	"paper_GetPapers";	out.send(); //a list of all papers (returns object_Object for every object)
		out["event"]=	"paper_GetClients";	out.send(); //a list of clients that are member of specified object. (returns object_Clients for every object)

		out["event"]=	"paper_ClientDraw";		out.send(); //draw something

		out["event"]=	"paper_Export";			out.send(); //export the paper to svg/png

		out["event"]=	"paper_GetList";			out.send(); //get a list of papers

		out["event"]=	"paper_Authenticate";			out.send(); //try authenticate ourselfs with specified key
		out["event"]=	"paper_ChangeAuth";			out.send(); //adds or changes authentication keys (only owner can do this offcourse)



		//client receive-only events:
		out.clear();
		out.event="core_ChangeEvent";
		out["modifyGroup"]=	"modules";
		out["sendGroup"]=	"modules";
		out["recvGroup"]=	"anonymous";
		out["event"]=	"object_Object";		out.send(); //result of a object_GetObjects. first and last objects are indicated. If a new object is created, a object_Object is also emitted.
		out["event"]=	"object_Deleted";		out.send(); //object you are member of has been deleted
		out["event"]=	"object_Client";		out.send(); //result of a object_GetClients. first and last clients are indicated. If a new client joins, a object_Client is also emitted.
		out["event"]=	"object_Joined";		out.send(); //send to client that has just joined the object
		out["event"]=	"object_Left";			out.send(); //somebody has left the object

		out["event"]=	"paper_CheckNotFound";	out.send(); //object not found
		out["event"]=	"paper_CheckOk";		out.send(); //object found and accesible

		out["event"]=	"paper_ServerDraw";		out.send(); //draw something

		out["event"]=	"paper_Exported";		out.send(); //paper is exported. (send for each exported type)

		out["event"]=	"paper_List";		out.send(); //list of papers

		out["event"]=	"paper_AuthWrongKey";		out.send(); //client tried to authenticate with wrong key
		out["event"]=	"paper_Authorized";		out.send(); //client is authorized new rights

		out.clear();
		out.event="core_LoadModule";
		out["path"]="modules/timer.module/libtimer.so";
		out.send();

		out["path"]="modules/http_json.module/libhttp_json.so";
		out.send();

		out["path"]="modules/exec.module/libexec.so";
		out.send();

		//tell the rest of the world we are ready for duty
		//(the core will send a timer_Ready)
		out.clear();
		out.event="core_Ready";
		out.send();

	}


	synapse::CobjectMan<CpaperObject> gObjectMan("var/paper");


	/** Client wants new paper.
	 * \P moveClients set this to one to move all the clients to the new paper.

	 * \note After creating, the creator has temporary owner rights. Dont forget to assign some kind of permanent key, otherwise the drawing will be unaccesible.

	 * \SEND object_Client
	 * Send to inform all the other clients of this new one
	 * Filled with parameters from \ref CpaperObject::getInfo

	 * \SEND object_Joined
	 * When moveClients is set to 1 it will send this to all clients.
	 * Filled with parameters from \ref CpaperObject::getInfo
	 *
	 *
	 */
	SYNAPSE_REGISTER(paper_Create)
	{
		int objectId;

		//move all clients with us?
		if (msg["moveClients"])
		{

			//get oldObjectId and create new object
			int oldObjectId=gObjectMan.getObjectByClient(msg.src).getId();
			objectId=gObjectMan.add();

			//now actually move the clients
			gObjectMan.moveClients(oldObjectId, objectId);
		}
		else
		{
			objectId=gObjectMan.add();
			gObjectMan.getObject(objectId).addClient(msg.src);
		}

		//give the creator temporary owner rights and inform the others about it
		//The client should add a key and rights as soon as possible, otherwise the drawing will be inaccessible after leaving it!
		Cvar rights;
		rights["owner"]=1;
		gObjectMan.getObject(objectId).getClient(msg.src).authorize(rights);
		gObjectMan.getObject(objectId).sendClientUpdate(msg.src);
	}

	/** Clients wants to delete a paper
	 * TODO: implement credentials first
	 * TODO: implement delete
	 */
	SYNAPSE_REGISTER(paper_Delete)
	{
		//objectMan.destroy(msg["objectId"]);
	}


	/** Try to authenticate and join a client to a paper.
	 * Can also be used to reauthenticate.
	 * \P objectId The paper to login to.
	 * \P key The key to authenticate with.
     *
	 * \SEND object_Joined
	 * Send to client to indicate they have joined a new object.
	 * Filled with parameters from \ref CpaperObject::getInfo
	 *
	 * \SEND object_Client
	 * When authentication succeeded.
	 * Send to all connected clients to indicate a new client has joined.
	 * Filled with info from \ref CpaperClient::getInfo
	 *
	 * \REPLY paper_AuthWrongKey
	 * When authentication has failed.
 	 */
	SYNAPSE_REGISTER(paper_Login)
	{
		gObjectMan.getObject(msg["objectId"]).login(msg.src,msg["key"]);
	}

	/** Change authentication and authorisation info
	 * 		\P key The key to change or add. Specify an empty key to set the default rights.
	 * 		\P rights See \ref CpaperClient::getInfo
	 *
	 * You need owner rights to do this.
	 */
	SYNAPSE_REGISTER(paper_ChangeAuth)
	{
		gObjectMan.getObjectByClient(msg.src).changeAuth(msg.src,msg["key"],msg["rights"]);
	}

	/** Client wants to check if the paper exists and credentials are ok
	 * TODO: credential stuff
	 *
	 */
	SYNAPSE_REGISTER(paper_Check)
	{
		Cmsg out;
		out=msg;
		out.src=msg.dst;
		out.dst=msg.src;

		try
		{
			//besides checking, we also recreate the html (for now)
			gObjectMan.getObject(msg["objectId"]).createHtml();
			gObjectMan.getObject(msg["objectId"]).getInfo(out);
			out.event="paper_CheckOk";
		}
		catch(...)
		{
			out.event="paper_CheckNotFound";
		}
		out.send();
	}

	/** Clients wants to leave the paper
	 *
	 */
	SYNAPSE_REGISTER(paper_Leave)
	{
		gObjectMan.getObject(msg["objectId"]).delClient(msg.src);
	}

	/** Client wants to receive a list of papers (those that are currently in memory!)
	 *
	 */
	SYNAPSE_REGISTER(paper_GetPapers)
	{
		gObjectMan.sendObjectList(msg.src);
	}

	/** Client wants to receive a fresh list of clients
	 *
	 */
//	SYNAPSE_REGISTER(paper_GetClients)
//	{
//		objectMan.getObjectByClient(msg.src).sendClientList(msg.src);
//	}

	SYNAPSE_REGISTER(module_SessionEnded)
	{
		gObjectMan.leaveAll(msg["session"]);
	}

	SYNAPSE_REGISTER(module_Shutdown)
	{
		gObjectMan.saveAll();
		gShutdown=true;
	}


	SYNAPSE_REGISTER(timer_Ready)
	{
		Cmsg out;
		out.clear();
		out.event="timer_Set";
		out["seconds"]=10;
		out["repeat"]=-1;
		out["dst"]=dst;
		out["event"]="paper_Timer";
		out.dst=msg["session"];
		out.send();
	}

	/** Timer to save unsaved stuff every X seconds
	 *
	 */
	SYNAPSE_REGISTER(paper_Timer)
	{
		gObjectMan.saveAll();
		gPaperIndex.save("var/paper/index");
	}


	/** Draw commands from the client
	 *
	 */
	SYNAPSE_REGISTER(paper_ClientDraw)
	{

		//NIET? anders zie je auth excetions niet..
//		try
//		{
			gObjectMan.getObjectByClient(msg.src).clientDraw(msg);
//		}
//		catch(...)
//		{
//			; //ignore exceptions, due to race conditions in deletes etc.
//		}
	}

	/*** Request paper to be saved and exported immeadiatly
	 *
	 */
	SYNAPSE_REGISTER(paper_Export)
	{
		gObjectMan.getObjectByClient(msg.src).saveExport();
	}


	SYNAPSE_REGISTER(exec_Ended)
	{
		if (msg.isSet("id"))
		{
			//tell the object the execution is complete
			gObjectMan.getObject(msg["id"]["paperId"]).execEnded(msg["id"]);

			//update the recently-changed index:
			Cvar paperInfo;
			gObjectMan.getObject(msg["id"]["paperId"]).getInfo(paperInfo);

			if (paperInfo["version"]>0)
			{
				//delete ourself from the list first
				for (synapse::CvarList::iterator paperI=gPaperIndex["list"].list().begin(); paperI!=gPaperIndex["list"].list().end(); paperI++)
				{
					if ((*paperI)["objectId"]==msg["id"]["paperId"])
					{
						gPaperIndex["list"].list().erase(paperI);
						break;
					}
				}

				//now re-add the updated version of ourself in the front position
				gPaperIndex["list"].list().push_front(paperInfo);

				//never more then this amount in the index
				if (gPaperIndex["list"].list().size()>100)
					gPaperIndex["list"].list().pop_back();

				gPaperIndex.changed();
			}

		}
	}


	SYNAPSE_REGISTER(exec_Error)
	{
		if (msg.isSet("id"))
		{
			gObjectMan.getObject(msg["id"]["paperId"]).execError(msg["id"]);
		}
	}

	SYNAPSE_REGISTER(paper_GetList)
	{
		Cmsg out;
		out.dst=msg.src;
		out.event="paper_List";
		out["list"]=gPaperIndex["list"];
		out["time"]=time(NULL);
		out.send();
	}

}

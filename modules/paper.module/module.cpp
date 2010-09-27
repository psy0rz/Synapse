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

//we use the generic shared object management classes.
#include "cclient.h"
#include "csharedobject.h"
#include "cobjectman.h"


namespace paper
{

	using namespace std;
	using namespace boost;
	using namespace boost::posix_time;

	bool shutdown;

	SYNAPSE_REGISTER(module_Init)
	{
		Cmsg out;

		shutdown=false;

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
		out["event"]=	"paper_Delete";		out.send(); //delete a object
		out["event"]=	"paper_Join";		out.send(); //join a object
		out["event"]=	"paper_Leave";		out.send(); //leave a object
		out["event"]=	"paper_GetPapers";	out.send(); //a list of all papers (returns object_Object for every object)
		out["event"]=	"paper_GetClients";	out.send(); //a list of clients that are member of specified object. (returns object_Clients for every object)

		out["event"]=	"paper_clientDraw";		out.send(); //draw something
		out["event"]=	"paper_clientMove";		out.send(); //just move mouse

		//client receive-only events:
		out.clear();
		out.event="core_ChangeEvent";
		out["modifyGroup"]=	"modules";
		out["sendGroup"]=	"modules";
		out["recvGroup"]=	"anonymous";
		out["event"]=	"object_Object";		out.send(); //result of a object_GetObjects. first and last objects are indicated. If a new object is created, a object_Object is also emitted.
		out["event"]=	"object_Deleted";		out.send(); //object you are member of has been deleted
		out["event"]=	"object_Client";		out.send(); //result of a object_GetClients. first and last clients are indicated. If a new client joins, a object_Client is also emitted.
		out["event"]=	"object_Left";			out.send(); //somebody has left the object

		out["event"]=	"paper_Status";			out.send();
		out["event"]=	"paper_serverDraw";		out.send(); //draw something
		out["event"]=	"paper_serverMove";		out.send(); //just move mouse


		//tell the rest of the world we are ready for duty
		//(the core will send a timer_Ready)
		out.clear();
		out.event="core_Ready";
		out.send();
	}

	//a client of a paper object
	class CpaperClient : public synapse::Cclient
	{
		public:
		char mode;
		int x;
		int y;
		int color;

		CpaperClient()
		{
			mode='m';
			x=0;
			y=0;
			color=0;
		}

	};


	//a server side piece of paper
	class CpaperObject : public synapse::CsharedObject<CpaperClient>
	{
		public:
		int lastClient;
		Cvar drawing;


		CpaperObject()
		{
			lastClient=0;
		}

		//add data to the drawing and send it (efficiently) to the clients
		void addDraw(Cmsg & msg)
		{
			Cmsg out;
			out.event="paper_serverDraw";

			//instructions come from a different client then last time?
			if (msg.src!=lastClient)
			{
				//inform every one of the client change
				out.list().push_back(string("i"));
				out.list().push_back(msg.src);
				lastClient=msg.src;
			}

			//get a reference to the client object
			//CpaperClient & paperClient=getClient(lastClient);


			//TODO: optimize by parsing and skipping stuff
			//for now just copy all the data
			out.list().insert(out.list().end(), msg.list().begin(), msg.list().end());

			//parse the draw-data
//			Cvar::iteratorList I;
//			I=msg.list().begin();
//			while(I!=msg.list().end())
//			{
//				//a change of mode
//				if (I->which()==CVAR_STRING)
//				{
//					//move
//					if (I->str()=="m")
//					{
//						if (paperClient.mode!='m')
//						{
//
//						}
//						I++;
//						continue;
//					}
//				}
//
//			}

			//send to all connected clients
			send(out);

			//store instructions permanently
			drawing.list().insert(drawing.list().end(), out.list().begin(), out.list().end());
		}

	};


	synapse::CobjectMan<CpaperObject> objectMan;



	///////////////////////////////////////////////////////////////////////////////////
	/// Generic object handlers, you can use this for other modules as well
	///////////////////////////////////////////////////////////////////////////////////

	SYNAPSE_REGISTER(paper_Create)
	{
		objectMan.create(msg.src, msg["clientName"], msg["objectName"]);
	}

	SYNAPSE_REGISTER(paper_Delete)
	{
		objectMan.destroy(msg["objectId"]);
	}

	SYNAPSE_REGISTER(paper_Join)
	{
		objectMan.getObject(msg["objectId"]).addClient(msg.src,msg["clientName"]);
	}

	SYNAPSE_REGISTER(paper_Leave)
	{
		objectMan.getObject(msg["objectId"]).delClient(msg.src);
	}

	SYNAPSE_REGISTER(paper_GetPapers)
	{
		objectMan.sendObjectList(msg.src);
	}

	SYNAPSE_REGISTER(paper_GetClients)
	{
		objectMan.getObject(msg["objectId"]).sendClientList(msg.src);
	}

	SYNAPSE_REGISTER(module_SessionEnded)
	{
		objectMan.leaveAll(msg["session"]);
	}

	SYNAPSE_REGISTER(module_Shutdown)
	{
		shutdown=true;
	}



	///////////////////////////////////////////////////////////////////////////////////
	/// Paper Module specific stuff
	///////////////////////////////////////////////////////////////////////////////////

	SYNAPSE_REGISTER(paper_clientDraw)
	{
		objectMan.getObjectByClient(msg.src).addDraw(msg);
	}

}

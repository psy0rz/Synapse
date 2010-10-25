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
		out["event"]=	"paper_Check";		out.send(); //check if object exists/access is allowed
		out["event"]=	"paper_Delete";		out.send(); //delete a object
		out["event"]=	"paper_Join";		out.send(); //join a object
		out["event"]=	"paper_Leave";		out.send(); //leave a object
		out["event"]=	"paper_GetPapers";	out.send(); //a list of all papers (returns object_Object for every object)
		out["event"]=	"paper_GetClients";	out.send(); //a list of clients that are member of specified object. (returns object_Clients for every object)

		out["event"]=	"paper_ClientDraw";		out.send(); //draw something
		out["event"]=	"paper_Redraw";		out.send(); //ask the server to send an entire redraw

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
		out["event"]=	"paper_CheckOk";	out.send(); //object found and accesible

		out["event"]=	"paper_Status";			out.send();
		out["event"]=	"paper_ServerDraw";		out.send(); //draw something

		out.clear();
		out.event="core_LoadModule";
		out["path"]="modules/timer.module/libtimer.so";
		out.send();

		out["path"]="modules/http_json.module/libhttp_json.so";
		out.send();


		//tell the rest of the world we are ready for duty
		//(the core will send a timer_Ready)
		out.clear();
		out.event="core_Ready";
		out.send();
	}




	//a client of a paper object
	class CpaperClient : public synapse::Cclient
	{
		friend class CpaperObject;
		private:

		public:
		Cvar settings;
		CvarList drawing;
		bool didDraw; //client did draw something?

		//parses and collect usefull drawing commands for this client.
		//returns true if the sharedobject should permanently store the collected data
		//NOTE: this is not really a "parser": it needs exactly one command with its parameters. The clients should make sure they send it this way.
		bool add(CvarList & commands )
		{
			if (commands.begin()->which()==CVAR_STRING)
			{
				//cancel drawing action (but keep settings!)
				if (commands.begin()->str()=="x")
				{
					drawing.clear();
				}
				//commit drawing action and settings
				else if (commands.begin()->str()=="s")
				{
					return(true);
				}
				//mouse movements, ignore
				else if (commands.begin()->str()=="m")
				{
					;
				}
				//drawing commands
				else if (
						(commands.begin()->str()=="l") ||
						(commands.begin()->str()=="r") ||
						(commands.begin()->str()=="a") ||
						(commands.begin()->str()=="t") ||
						(commands.begin()->str()==".")
				)
				{
					drawing.insert(drawing.end(), commands.begin(), commands.end());
				}
				//drawing settings
				else if (
						(commands.begin()->str()=="c") ||
						(commands.begin()->str()=="w") ||
						(commands.begin()->str()=="n")
				)
				{
					settings[commands.begin()->str()]=(++commands.begin())->str();
				}
				//delete
				else if (commands.begin()->str()=="D")
				{
					drawing.clear();
					settings.clear();
					if (didDraw)
					{
						//if the client did do SOMETHING worth storing, then store the Delete command as well
						drawing.push_back(string("D"));
						return(true);
					}
					else
						return(false);
				}
				else
				{
					;//ignore the rest for now
				}

			}
			//numbers, just add them to the drawing
			else
			{
				drawing.insert(drawing.end(), commands.begin(), commands.end());
			}
			return(false);
		}

		//add current values to specified drawing
		bool store(CvarList & addDrawing)
		{
			bool added=false;
			//store new settings
			for(Cvar::iterator I=settings.begin(); I!=settings.end(); I++)
			{
				addDrawing.push_back(I->first);
				addDrawing.push_back(I->second);
				added=true;
			}

			//store drawing commands
			if (!drawing.empty())
			{
				addDrawing.insert(addDrawing.end(), drawing.begin(), drawing.end());
				added=true;
			}

			return(added);
		}

		//store current values and forget everything
		void commit(CvarList & addDrawing)
		{
			if (store(addDrawing))
				didDraw=true;
			settings.clear();
			drawing.clear();
		}


		CpaperClient()
		{
			didDraw=false;
		}

	};


	//a server side piece of paper
	class CpaperObject : public synapse::CsharedObject<CpaperClient>
	{
		public:
		int lastSentClient;
		int lastStoreClient;
		synapse::Cconfig drawing;

		CpaperObject()
		{
			lastSentClient=0;
			lastStoreClient=0;
		}

		void save(string path)
		{
			drawing.save(path);
			saved=true;
		}

		void load(string path)
		{
			drawing.load(path);
			saved=true;

			//"fsck" for stale clients (they should only exist if we where aborted or crashing)
			set<int> ids;
			int lastId;
			CvarList & drawingData=drawing["data"].list();
			CvarList::iterator I=drawingData.begin();
			//parse all the clients that joined and left:
			while (I!=drawingData.end())
			{
				if (I->which()==CVAR_STRING)
				{
					//new client
					if (I->str()=="I")
					{
						I++;
						lastId=*I;
						ids.insert(lastId);
					}
					//delete last selected client:
					else if (I->str()=="D")
					{
						ids.erase(lastId);
					}
				}
				I++;
			}

			//there should be nothing left, otherwise fix it by adding appropriate deletes:
			while(!ids.empty())
			{
				WARNING("Fixed stale client in drawing: " << *ids.begin())
				drawingData.push_back(string("I"));
				drawingData.push_back(*ids.begin());
				drawingData.push_back(string("D"));
				ids.erase(ids.begin());
			}

		}


		//send the commands to the clients and store permanently if neccesary.
		//on behalf of clientId (use clientId 0 for global commands)
		void serverDraw(CvarList & commands, int clientId=0 )
		{
			Cmsg out;
			out.event="paper_ServerDraw";

			//instructions come from a different client then last time?
			if (clientId && clientId!=lastSentClient)
			{
				//add client change commands
				out.list().push_back(string("I"));
				out.list().push_back(clientId);
				lastSentClient=clientId;
			}

			//add commands to output message
			out.list().insert(out.list().end(), commands.begin(), commands.end());

			//send to all connected clients, except to skipDst
			for (CclientMap::iterator I=clientMap.begin(); I!=clientMap.end(); I++)
			{
				if (I->first!=clientId)
				{
					out.dst=I->first;
					try
					{
						out.send();
					}
					catch(...)
					{
						//ignore send errors (expectable race conditions do occur with session ending)
						;
					}
				}
			}

			//a global command?
			if (!clientId)
			{
				//always just store it
				drawing["data"].list().insert(drawing["data"].list().end(), commands.begin(), commands.end());
			}
			//a command from specific client?
			else
			{
				//parse drawing and cache instructions for this client
				if (getClient(clientId).add(commands))
				{
					//client object says its ready to commit, store permanently

					//its a different client as the last one we've stored?
					if (lastStoreClient!=clientId)
					{
						//no, so store client-switch instruction:
						drawing["data"].list().push_back(string("I"));
						drawing["data"].list().push_back(clientId);
						lastStoreClient=clientId;
					}

					//commit drawing instructions of this client
					getClient(clientId).commit(drawing["data"].list());
					saved=false;
				}
			}

		}


		//process drawing data received from a client and pass it to serverDraw for furher processing.
		void clientDraw(Cmsg & msg)
		{
//			CvarList commands;
//
//			//copy the received commands
//			commands.insert(commands.end(), msg.list().begin(), msg.list().end());

			//sends the commands to other clients and store
			serverDraw(msg.list(),msg.src);

		}

		virtual void delClient(int id)
		{
			if (clientMap.find(id)!= clientMap.end())
			{
				//do a Del command on behalf of the client
				CvarList commands;
				commands.push_back(string("D"));
				serverDraw(commands,id);
				//lastClient=0;
//				if (getClient(id).didDraw)
	//				lastStoreClient=0;
				synapse::CsharedObject<CpaperClient>::delClient(id);
			}
		}


		//send redrawing instructions to dst
		void redraw(int dst)
		{
			Cmsg out;
			out.event="paper_ServerDraw";
			out.dst=dst;
			out.list().push_back(string("S"));
			out.list().insert(out.list().end(), drawing["data"].list().begin(), drawing["data"].list().end());
			out.list().push_back(string("E"));

			//add current uncommited stuff of all clients
			for (CclientMap::iterator I=clientMap.begin(); I!=clientMap.end(); I++)
			{
				out.list().push_back(string("I"));
				out.list().push_back(I->first);
				I->second.store(out.list());
			}

			//switch back to current client
//			if (lastClient)
//			{
//				out.list().push_back(string("I"));
//				out.list().push_back(lastClient);
//			}

			out.send();

		}

		virtual void addClient(int id)
		{
			//let the base class do its work:
			synapse::CsharedObject<CpaperClient>::addClient(id);
			//make sure redraw commands are send BEFORE any other draw commands
			redraw(id);

		}


	};



	synapse::CobjectMan<CpaperObject> objectMan("var/paper");


	/** Client wants new paper
	 *
	 */
	SYNAPSE_REGISTER(paper_Create)
	{
		//move all clients with us?
		if (msg["moveClients"])
		{

			//get oldObjectId and create new object
			int oldObjectId=objectMan.getObjectByClient(msg.src).getId();
			int newObjectId=objectMan.add();

			//store reference to next object in the old one..
			CvarList commands;
			commands.push_back(string("N"));
			commands.push_back(newObjectId);
			objectMan.getObject(oldObjectId).serverDraw(commands);

			//store reference to previous object in the new one..
			commands.clear();
			commands.push_back(string("P"));
			commands.push_back(oldObjectId);
			objectMan.getObject(newObjectId).serverDraw(commands);

			//now actually move the clients
			objectMan.moveClients(oldObjectId, newObjectId);
		}
		else
		{
			int objectId=objectMan.add();
			objectMan.getObject(objectId).addClient(msg.src);
		}
	}

	/** Clients wants to delete a paper
	 * TODO: implement credentials first
	 * TODO: implement delete
	 */
//	SYNAPSE_REGISTER(paper_Delete)
//	{
//		objectMan.destroy(msg["objectId"]);
//	}

	/** Client wants to join a paper
	 *
	 */
	SYNAPSE_REGISTER(paper_Join)
	{
		objectMan.leaveAll(msg.src); //remove this if you want clients to be able to join multiple objects
		objectMan.getObject(msg["objectId"]).addClient(msg.src);

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
			objectMan.getObject(msg["objectId"]);
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
		objectMan.getObject(msg["objectId"]).delClient(msg.src);
	}

	/** Client wants to receive a list of papers (those that are currently in memory!)
	 *
	 */
	SYNAPSE_REGISTER(paper_GetPapers)
	{
		objectMan.sendObjectList(msg.src);
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
		objectMan.leaveAll(msg["session"]);
	}

	SYNAPSE_REGISTER(module_Shutdown)
	{
		objectMan.saveAll();
		shutdown=true;
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
		objectMan.saveAll();
	}


	/** Draw commands from the client
	 *
	 */
	SYNAPSE_REGISTER(paper_ClientDraw)
	{
		objectMan.getObjectByClient(msg.src).clientDraw(msg);
	}





}

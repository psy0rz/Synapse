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


namespace pong
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
		out["event"]=	"object_Create";		out.send(); //create new object
		out["event"]=	"object_Delete";		out.send(); //delete a object
		out["event"]=	"object_Join";			out.send(); //join a object
		out["event"]=	"object_Leave";			out.send(); //leave a object
		out["event"]=	"object_GetObjects";	out.send(); //a list of all objects (returns object_Object for every object)
		out["event"]=	"object_GetClients";	out.send(); //a list of clients that are member of specified object. (returns object_Clients for every object)

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

	///////////////////////////////////////////////////////////////////////////////////
	/// Generic object management classes, you can use this for other modules as well
	///////////////////////////////////////////////////////////////////////////////////


	class Cclient
	{
		private:
		string name;


		public:

		Cclient()
		{
		}

		const string & getName()
		{
			return(name);
		}

		void setName(string name)
		{
			this->name=name;
		}
	};


	class CsharedObject
	{

		int id;
		string name;

		typedef map<int,Cclient> CclientMap;
		CclientMap clientMap;


		public:

		CsharedObject()
		{
		}

		const string & getName()
		{
			return(name);
		}

		//send a message to all clients of the object
		void send(Cmsg & msg)
		{
			for (CclientMap::iterator I=clientMap.begin(); I!=clientMap.end(); I++)
			{
				msg.dst=I->first;
				msg.send();
			}
		}


		void create(int id, string name)
		{
			this->id=id;
			this->name=name;
		}

		void getInfo(Cmsg & msg)
		{
			msg["objectId"]=id;
			msg["objectName"]=name;
		}

		//sends a client list to specified destination
		void sendClientList(int dst)
		{
			Cmsg out;
			out.event="object_Client";
			out["objectId"]=id;
			out["first"]=1;
			out.dst=dst;

			for (CclientMap::iterator I=clientMap.begin(); I!=clientMap.end(); I++)
			{
				if (I==(--clientMap.end()))
					out["last"]=1;

				out["clientId"]=I->first;
				out["clientName"]=I->second.getName();
				out.send();

				out.erase("first");
			}
		}

		void addClient(int id, string name)
		{
			if (clientMap.find(id)==clientMap.end())
			{
				if (name=="")
				{
					throw(runtime_error("Please enter a name before joining."));
				}

				clientMap[id].setName(name);
				Cmsg out;
				out.event="object_Client";
				out["clientId"]=id;
				out["objectId"]=this->id;
				send(out); //inform all members of the new client
			}
			else
			{
				throw(runtime_error("You're already joined?"));
			}
		}

		void delClient(int id)
		{
			if (clientMap.find(id)!= clientMap.end())
			{
				clientMap.erase(id);

				Cmsg out;
				out.event="object_Left";
				out["clientId"]=id;
				out["objectId"]=this->id;
				send(out); //inform all members of the new client
			}
		}

		void destroy()
		{
			Cmsg out;
			out.event="object_Deleted";
			out["objectId"]=this->id;
			send(out); //inform all members

			//delete all joined clients:
			for (CclientMap::iterator I=clientMap.begin(); I!=clientMap.end(); I++)
			{
				delClient(I->first);
			}
		}


	};


	class CobjectMan
	{
		private:

		typedef map<int,CsharedObject> CobjectMap ;
		CobjectMap objectMap;

		int lastId;

		public:

		CobjectMan()
		{
			lastId=0;
		}


		// Get a reference to an object or throw exception
		CsharedObject & getObject(int id)
		{
			if (objectMap.find(id)== objectMap.end())
				throw(runtime_error("Object not found!"));

			return (objectMap[id]);
		}


		void create(int clientId, string clientName, string objectName)
		{
			if (objectName=="")
				throw(runtime_error("Please specify the object name!"));

			if (clientName=="")
				throw(runtime_error("Please specify your name!"));

			lastId++;
			objectMap[lastId].create(lastId,clientName);
			objectMap[lastId].addClient(clientId, clientName);

			//send the object to the requester
			Cmsg out;
			out.event="object_Object";
			objectMap[lastId].getInfo(out);
			out.dst=clientId;
			out.send();
		}

		void destroy(int objectId)
		{
			getObject(objectId).destroy();
			objectMap.erase(objectId);
		}

		//sends a object list to specified destination
		void sendObjectList(int dst)
		{
			Cmsg out;
			out.event="object_Object";
			out["first"]=1;
			out.dst=dst;

			for (CobjectMap::iterator I=objectMap.begin(); I!=objectMap.end(); I++)
			{
				if (I==(--objectMap.end()))
					out["last"]=1;

				I->second.getInfo(out);
				out.send();

				out.erase("first");
			}
		}

		void leaveAll(int clientId)
		{
			for (CobjectMap::iterator I=objectMap.begin(); I!=objectMap.end(); I++)
			{
				I->second.delClient(clientId);
			}

		}
	};

	CobjectMan objectMan;
//	mutex threadMutex;


	///////////////////////////////////////////////////////////////////////////////////
	/// Generic object handlers, you can use this for other modules as well
	///////////////////////////////////////////////////////////////////////////////////

	SYNAPSE_REGISTER(object_Create)
	{
		objectMan.create(msg.src, msg["clientName"], msg["objectName"]);
	}

	SYNAPSE_REGISTER(object_Delete)
	{
		objectMan.destroy(msg["objectId"]);
	}

	SYNAPSE_REGISTER(object_Join)
	{
		objectMan.getObject(msg["objectId"]).addClient(msg.src,msg["clientName"]);
	}

	SYNAPSE_REGISTER(object_Leave)
	{
		objectMan.getObject(msg["objectId"]).delClient(msg.src);
	}

	SYNAPSE_REGISTER(object_GetObjects)
	{
		objectMan.sendObjectList(msg.src);
	}

	SYNAPSE_REGISTER(object_GetClients)
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

//	SYNAPSE_REGISTER(paper_clientDraw)
//	{
//		Cmsg out;
//		out.event="paper_serverDraw";
//		out["objectId"]=msg["objectId"];
//		out["x1"]=msg["x1"];
//		objectMan.getObject(msg["objectId"]).send(out);
//	}

}

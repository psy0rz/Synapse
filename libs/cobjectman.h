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

/**
 * These classes are for managing server side objects that can be shared by multiple clients.
 * This can be used for games, to allow players to "create" a game and let other players join that game.
 * Its also used by the paper-module.
 */

#ifndef COBJECTMAN_H_
#define COBJECTMAN_H_

#include <map>
#include "cconfig.h"

namespace synapse
{
	using namespace std;

	template <typename TsharedObject >
	class CobjectMan
	{
		private:

		typedef map<int,TsharedObject > CobjectMap ;
		CobjectMap objectMap;

		string storagePath;

		protected:
		Cconfig config;

		public:

		CobjectMan(string path="")
		{
			storagePath=path;
			config["lastId"]=0;

			if (storagePath!="")
			{
				INFO("Loading object config from " << storagePath << "/config");
				config.load(storagePath+"/config",true);
			}
		}


		// Get a reference to an object, load it from disk if neccesary, or throw exception
		TsharedObject & getObject(int objectId)
		{
			if (objectMap.find(objectId)== objectMap.end())
			{
				if (getStoragePath(objectId)!="")
				{
					DEB("Loading data for object " << objectId << " from " << getStoragePath(objectId));
					TsharedObject object;
					object.create(objectId);
					object.load(getStoragePath(objectId));
					objectMap[objectId]=object;
				}
				else
				{
					throw(runtime_error("Object not found!"));
				}
			}

			return (objectMap[objectId]);
		}

		// Get a reference to an object by client id.
		//TODO: OPTIMIZE: will get slow with a lot of objects? howeever not all objects are loaded all the time
		TsharedObject & getObjectByClient(int clientId)
		{

			for (typename CobjectMap::iterator I=objectMap.begin(); I!=objectMap.end(); I++)
			{
				if (I->second.clientExists(clientId))
				{
					return (objectMap[I->first]);
				}
			}

			throw(runtime_error("You are not joined to an object!"));
		}


		int add()
		{

			config["lastId"]=config["lastId"]+1;
			config.changed();

			objectMap[config["lastId"]].create(config["lastId"]);
			return (config["lastId"]);

//			//send the object to the requester
//			Cmsg out;
//			out.event="object_Object";
//			objectMap[config["lastId"]].getInfo(out);
//			out["new"]=1;
//			out.dst=clientId;
//			out.send();
		}

		string getStoragePath(int objectId)
		{
			if (storagePath=="")
				return ("");
			stringstream path;
			path << storagePath << "/" << objectId;
			return(path.str());
		}

		//save the object to disk
		void save(int objectId)
		{
			//DEB("Saving object " << objectId << " to " << getStoragePath(objectId));
			getObject(objectId).save(getStoragePath(objectId));
		}

		//deletes object from memory.
		void unload(int objectId)
		{
			DEB("Unloading object " << objectId);
			objectMap.erase(objectId);
		}

		//sends a object list to specified destination
		//NOTE: Only sends a list of objects that are currently loaded.
		void sendObjectList(int dst)
		{
			Cmsg out;
			out.event="object_Object";
			out["first"]=1;
			out.dst=dst;

			for (typename CobjectMap::iterator I=objectMap.begin(); I!=objectMap.end(); I++)
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
			for (typename CobjectMap::iterator I=objectMap.begin(); I!=objectMap.end(); I++)
			{
				I->second.delClient(clientId);
			}

		}

		void moveClients(int oldObjectId, int newObjectId)
		{
			set<int> ids=getObject(oldObjectId).delAllClients();
			getObject(newObjectId).addClients(ids);
		}


		void saveAll()
		{
			//main config:
			config.save(storagePath+"/config");

			list<int> unloads;

			//per object config:
			for (typename CobjectMap::iterator I=objectMap.begin(); I!=objectMap.end(); I++)
			{
				try
				{
					//NOTE: save is smart and only saves if data has changed().
					save(I->first);

					if (I->second.isIdle())
						unloads.push_back(I->first);

				}
				catch(...)
				{
					ERROR("Error while saving object " << I->first);
				}

			}

			//now unload (this invalidates iterators, so we do it in a seperate step
			for (list<int>::iterator I=unloads.begin(); I!=unloads.end(); I++)
			{
				unload(*I);
			}


		}

	};

}

#endif

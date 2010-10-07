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

		Cconfig config;
		string storagePath;


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
		//FIXME: unload objects automatically
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
		//TODO: OPTIMIZE: will get slow with a lot of objects
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


		void add(int clientId)
		{

			config["lastId"]=config["lastId"]+1;

			objectMap[config["lastId"]].create(config["lastId"]);
			objectMap[config["lastId"]].addClient(clientId);

			//send the object to the requester
			Cmsg out;
			out.event="object_Object";
			objectMap[config["lastId"]].getInfo(out);
			out.dst=clientId;
			out.send();
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
			DEB("Saving object " << objectId << " to " << getStoragePath(objectId));
			getObject(objectId).save(getStoragePath(objectId));
			//now the lastId needs to be remembered as well:
			config.save(storagePath+"/config");
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

		void saveAll()
		{
			for (typename CobjectMap::iterator I=objectMap.begin(); I!=objectMap.end(); I++)
			{
				if (!I->second.isSaved())
				{
					try
					{
						save(I->first);

						//if the object doesnt have any clients anymore, unload it from memory.
						if (I->second.noClients())
							unload(I->first);
					}
					catch(...)
					{
						ERROR("Error while saving object " << I->first);
					}
				}
			}

		}

	};

}

#endif

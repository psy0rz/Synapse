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

namespace synapse
{
	using namespace std;

	template <typename TsharedObject >
	class CobjectMan
	{
		private:

		typedef map<int,TsharedObject > CobjectMap ;
		CobjectMap objectMap;

		int lastId;

		public:

		CobjectMan()
		{
			lastId=0;
		}


		// Get a reference to an object or throw exception
		TsharedObject & getObject(int id)
		{
			if (objectMap.find(id)== objectMap.end())
				throw(runtime_error("Object not found!"));

			return (objectMap[id]);
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


		void create(int clientId)
		{

			lastId++;
			objectMap[lastId].create(lastId);
			objectMap[lastId].addClient(clientId);

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
	};

}

#endif

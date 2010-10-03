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



#ifndef CSHAREDOBJECT_H_
#define CSHAREDOBJECT_H_
namespace synapse
{
	using namespace std;

	template <typename Tclient>
	class CsharedObject
	{

		int id;

		protected:
		typedef map<int,Tclient> CclientMap;
		CclientMap clientMap;


		public:

		CsharedObject()
		{
		}

		//send a message to all clients of the object
		void send(Cmsg & msg)
		{
			typename CclientMap::iterator I;
			for (I=clientMap.begin(); I!=clientMap.end(); I++)
			{
				msg.dst=I->first;
				msg.send();
			}
		}


		void create(int id)
		{
			this->id=id;
		}

		virtual void getInfo(Cmsg & msg)
		{
			msg["objectId"]=id;
		}

		virtual void save(string path)
		{
			throw(runtime_error("Programming error: This object doesnt support saving to disk (yet?)"));
		}

		virtual void load(string path)
		{
			throw(runtime_error("Programming error: This object doesnt support loading from disk (yet?)"));
		}

		//sends a client list to specified destination
		void sendClientList(int dst)
		{
			Cmsg out;
			out.event="object_Client";
			getInfo(out);
			out["first"]=1;
			out.dst=dst;

			typename CclientMap::iterator I;
			for (I=clientMap.begin(); I!=clientMap.end(); I++)
			{
				if (I==(--clientMap.end()))
					out["last"]=1;

				I->second.getInfo(out);
				out.send();

				out.erase("first");
			}
		}

		void addClient(int id)
		{
			if (clientMap.find(id)==clientMap.end())
			{
				clientMap[id].id=id;
				Cmsg out;
				out.event="object_Client";
				clientMap[id].getInfo(out);
				getInfo(out);
				send(out); //inform all members of the new client
			}
			else
			{
				throw(runtime_error("You're already joined?"));
			}
		}

		bool clientExists(int id)
		{
			return(clientMap.find(id)!=clientMap.end());
		}

		virtual void delClient(int id)
		{
			if (clientMap.find(id)!= clientMap.end())
			{

				Cmsg out;
				out.event="object_Left";
				getInfo(out);
				clientMap[id].getInfo(out);

				clientMap.erase(id);
				send(out); //inform all members of the left client
			}
		}

		// Get a reference to a client or throw exception
		Tclient & getClient(int id)
		{
			if (clientMap.find(id)== clientMap.end())
				throw(runtime_error("Client not found in this object!"));

			return (clientMap[id]);
		}


		void destroy()
		{
			Cmsg out;
			out.event="object_Deleted";
			getInfo(out);
			send(out); //inform all members

			//delete all joined clients:
			for (typename CclientMap::iterator I=clientMap.begin(); I!=clientMap.end(); I++)
			{
				delClient(I->first);
			}
		}


	};
};


#endif /* CSHAREDOBJECT_H_ */

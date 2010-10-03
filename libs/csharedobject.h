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
		string name;

		protected:
		typedef map<int,Tclient> CclientMap;
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
			typename CclientMap::iterator I;
			for (I=clientMap.begin(); I!=clientMap.end(); I++)
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

			typename CclientMap::iterator I;
			for (I=clientMap.begin(); I!=clientMap.end(); I++)
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

		bool clientExists(int id)
		{
			return(clientMap.find(id)!=clientMap.end());
		}

		virtual void delClient(int id)
		{
			if (clientMap.find(id)!= clientMap.end())
			{
				clientMap.erase(id);

				Cmsg out;
				out.event="object_Left";
				out["clientId"]=id;
				out["objectId"]=this->id;
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
			out["objectId"]=this->id;
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

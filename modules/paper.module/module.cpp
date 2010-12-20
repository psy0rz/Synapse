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
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/fstream.hpp"

//we use the generic shared object management classes.
#include "cclient.h"
#include "csharedobject.h"
#include "cobjectman.h"


namespace paper
{

	using namespace std;
	using namespace boost;
	using namespace boost::posix_time;
	using boost::filesystem::ofstream;
	using boost::filesystem::ifstream;

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

		out["path"]="modules/exec.module/libexec.so";
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
		Cvar cursor;
		int lastElementId;

		CpaperClient()
		{
			lastElementId=0;
		}
	};


	//a server side piece of paper
	class CpaperObject : public synapse::CsharedObject<CpaperClient>
	{
		public:
		synapse::Cconfig drawing;


		void save(string path)
		{
			if (drawing.isChanged())
				{
				drawing.save(path);

				//export to svg
				ofstream svgStream;
				svgStream.exceptions ( ofstream::eofbit | ofstream::failbit | ofstream::badbit );
				stringstream svgFilename;
				svgFilename << "wwwdir/p/" << id << ".svg";
				svgStream.open(svgFilename.str());

				//svg header
				svgStream << "<?xml version=\"1.0\" standalone=\"no\"?>\n";
				svgStream << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n\n";

//				svgStream << "<svg width=\"100%\" height=\"100%\" version=\"1.1\"";
//
//				//default settings
//				svgStream << " viewBox=\"0 0 17777 10000\"";
//				svgStream << " stroke-linecap=\"round\"";
//				svgStream << " stroke-linejoin=\"round\"";
////				svgStream << " preserveAspectRatio=\"none\"\n";
//				svgStream << " xmlns=\"http://www.w3.org/2000/svg\">\n\n";


				//export all elements
				for(Cvar::iterator elementI=drawing["data"].begin(); elementI!=drawing["data"].end(); elementI++)
				{
					//start element
					svgStream << "<" << elementI->second["element"].str();

					//add id
					svgStream << " id=\"" << elementI->first << "\"";

					//fill in all element attributes
					for(Cvar::iterator I=elementI->second.begin(); I!=elementI->second.end(); I++)
					{
						if (I->first!="element" && I->first!="text")
							svgStream << " " << I->first << "=\"" << I->second.str() << "\"";
					}

					//text content?
					if (elementI->second.isSet("text"))
					{
						//add text content and closing tag
						svgStream << ">" << elementI->second["text"].str() << "</" << elementI->second["element"].str() << ">\n";
					}
					//root svg element?
					else if (elementI->second["element"].str()=="svg")
					{
						//close, but dont end element
						svgStream << ">\n";
					}
					else
					{
						//end element
						svgStream << "/>\n";
					}
				}

				svgStream << "<title>internetpapier.nl tekening #" << id << "</title>\n";

				//close svg element
				svgStream << "\n</svg>\n";

				svgStream.close();


				//now let imagemagic convert it to some nice pngs :)
				stringstream pngFilename;
				Cmsg out;

				pngFilename.flush();
				pngFilename << "wwwdir/p/" << id;
				out.event="exec_Start";
				out["cmd"]="nice convert -density 10 " + svgFilename.str() + " " + pngFilename.str() + ".large.png";
				out.send();

//				out.event="exec_Start";
//				out["cmd"]="nice convert -density 2 " + svgFilename.str() + " " + pngFilename.str() + ".medium.png";
//				out.send();

//				out.event="exec_Start";
//				out["cmd"]="nice convert -density 1 " + svgFilename.str() + " " + pngFilename.str() + ".small.png";
//				out.send();
			}
		}

		void load(string path)
		{
			drawing.load(path);
		}


		//send the commands to the clients and store permanently if neccesary.
		//on behalf of clientId (use clientId 0 for global commands)
		void serverDraw(Cmsg out, int clientId=0 )
		{
			out.event="paper_ServerDraw";
			out.src=0;

			//send to all connected clients, but not back to the original clientId
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


		}




		//get an iterator to specified element id.
		//throws error if not found.
		Cvar::iterator getElement(const string & id)
		{
			Cvar::iterator elementI;
			elementI=drawing["data"].map().find(id);
			if (elementI==drawing["data"].map().end())
				throw(runtime_error("Specified element id not found."));
			return (elementI);
		}


		//transform the specified elementid into a message-data that can be send to clients
		//sets:
		//["element"]=elementtype
		//["set"][attributename]=attributevalue
		//["id"]=id
		//["beforeId"]=id of element this element comes before
		void element2msg(const string & id, Cmsg & msg)
		{
			//get an iterator to requested id
			Cvar::iterator elementI=getElement(id);

			//fill in element type
			msg["element"]=elementI->second["element"];
			msg["id"]=id;

			//fill in all element attributes
			for(Cvar::iterator I=elementI->second.begin(); I!=elementI->second.end(); I++)
			{
				if (I->first!="element")
					msg["set"][I->first]=I->second;
			}

			//is there an item after this?
			elementI++;
			if (elementI!=drawing["data"].end())
			{
				//yes, so fill in beforeId,  so the objects stay in the correct order
				msg["beforeId"]=elementI->first;
			}
		}

		//process drawing data received from a client store it, and relay it to other clients via serverDraw
		void clientDraw(Cmsg & msg)
		{
			msg["src"]=msg.src;

			//received cursor information?
			if (msg.isSet("cursor"))
			{
				//store the new fields in the clients cursor object:
				Cvar & cursor=getClient(msg.src).cursor;
				for(Cvar::iterator I=msg["cursor"].begin(); I!=msg["cursor"].end(); I++)
				{
					cursor[I->first]=I->second.str();
				}
			}

			//received chat?
			if (msg.isSet("chat"))
			{
				//store in chat log for this object
				drawing["chat"].list().push_back(msg["chat"]);
			}

			//received drawing commands?
			if (msg["cmd"].str()=="update")
			{
				//id specified?
				if (msg.isSet("id"))
				{
					//add new object?
					if (msg["id"].str()=="new")
					{
						//figure out a fresh new id
						drawing["lastElementId"]=drawing["lastElementId"]+1;

						//store the new id in the message
						msg["id"]=drawing["lastElementId"];

						//create new element
						drawing["data"][msg["id"]]["element"]=msg["element"].str();

					}

					//store the last id for this client
					getClient(msg.src).lastElementId=msg["id"];
				}
				//id not specified, automaticly add the last used id from this client
				else
				{
					msg["id"]=getClient(msg.src).lastElementId;
				}


				Cvar::iterator elementI=getElement(msg["id"]);

				//add the data from the 'add' field to the drawing data
				for(Cvar::iterator I=msg["add"].begin(); I!=msg["add"].end(); I++)
				{
					//this basically means: drawing["data"][key]+=value;
					elementI->second[I->first].str()+=I->second.str();
				}

				//set the data from the 'set' field over the drawing data
				for(Cvar::iterator I=msg["set"].begin(); I!=msg["set"].end(); I++)
				{
					elementI->second[I->first]=I->second.str();
				}

				drawing.changed();
			}
			//send refresh?
			else if (msg["cmd"].str()=="refresh")
			{
				//id not set? the use the last one that was used by this client
				if (!msg.isSet("id"))
				{
					if (getClient(msg.src).lastElementId)
						msg["id"]=getClient(msg.src).lastElementId;
				}

				//reply with a serverdraw to this client only
				Cmsg out;
				out=msg;
				out.event="paper_ServerDraw";
				out.dst=msg.src;
				out.src=0;
				out["cmd"]="update";
				if (msg.isSet("id"))
					element2msg(msg["id"].str(), out);
				out.send();
			}
			//delete object?
			else if (msg["cmd"].str()=="delete")
			{
				//delete it
				Cvar::iterator elementI=getElement(msg["id"]);
				drawing["data"].map().erase(elementI);
				drawing.changed();

				getClient(msg.src).lastElementId=0;
			}


			//relay the command to other clients
			if (!msg.isSet("norelay"))
				serverDraw(msg,msg.src);
		}

//		virtual void delClient(int id)
//		{
//			if (clientMap.find(id)!= clientMap.end())
//			{
//				//do a Del command on behalf of the client
//				CvarList commands;
//				commands.push_back(string("D"));
//				//serverDraw(commands,id);
//				//lastClient=0;
////				if (getClient(id).didDraw)
//	//				lastStoreClient=0;
//				synapse::CsharedObject<CpaperClient>::delClient(id);
//			}
//		}



		CpaperObject()
		{
			//1000r will be the root svg element with its settings
			//NOTE: svgweb doesnt support a numeric svg-root, hence the added r
			//NOTE: This is a STL ordered MAP, we need to keep the correct order, so hence the 1000.
			drawing["data"]["1000r"]["element"]="svg";
			drawing["data"]["1000r"]["version"]="1.2";
			drawing["data"]["1000r"]["baseProfile"]="tiny";
			drawing["data"]["1000r"]["viewBox"]="0 0 17777 10000";
			drawing["data"]["1000r"]["width"]="100%";
			drawing["data"]["1000r"]["height"]="100%";

			drawing["data"]["1000r"]["xmlns"]="http://www.w3.org/2000/svg";
			drawing["data"]["1000r"]["xmlns:xlink"]="http://www.w3.org/1999/xlink";
			//we dont use this YET:
			//drawing["data"]["1000r"]["xmlns:ev"]="http://www.w3.org/2001/xml-events";

			drawing["data"]["1000r"]["stroke-linecap"]="round";
			drawing["data"]["1000r"]["stroke-linejoin"]="round";


			//drawing.setAttribute("preserveAspectRatio", "none");
//				drawing.setAttribute("pointer-events","all");
//				drawing.setAttribute("color-rendering","optimizeSpeed");
//				drawing.setAttribute("shape-rendering","optimizeSpeed");
//				drawing.setAttribute("text-rendering","optimizeSpeed");
//				drawing.setAttribute("image-rendering","optimizeSpeed");

			//we start at 1001 so the order stays correct in the stl map. once we get to 10000 the order gets screwed up, but that probably never happens ;)
			drawing["lastElementId"]=1000;

		}

		//send redrawing instructions to dst
		void redraw(int dst)
		{
			Cmsg out;
			out.event="paper_ServerDraw";
			out.dst=dst;

			//send all elements
			for(Cvar::iterator elementI=drawing["data"].begin(); elementI!=drawing["data"].end(); elementI++)
			{
				out.clear();
				out["cmd"]="update";
				element2msg(elementI->first, out);
				out.send();
			}

			//send all cursors
			for(CclientMap::iterator I=clientMap.begin(); I!=clientMap.end(); I++)
			{
				out.clear();
				out["cursor"].map()=I->second.cursor;
				out["src"]=I->first;
				out.send();
			}

			//send chat log
			for(CvarList::iterator I=drawing["chat"].list().begin(); I!=drawing["chat"].list().end(); I++)
			{
				out.clear();
				out["chat"]=*I;
				out.send();
			}


			out.clear();
			out["cmd"]="ready";
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

//			//store reference to next object in the old one..
//			CvarList commands;
//			commands.push_back(string("N"));
//			commands.push_back(newObjectId);
//			//objectMan.getObject(oldObjectId).serverDraw(commands);
//
//			//store reference to previous object in the new one..
//			commands.clear();
//			commands.push_back(string("P"));
//			commands.push_back(oldObjectId);
//			//objectMan.getObject(newObjectId).serverDraw(commands);

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
		try
		{
			objectMan.getObjectByClient(msg.src).clientDraw(msg);
		}
		catch(...)
		{
			; //ignore exceptions, due to race conditions in deletes etc.
		}
	}





}

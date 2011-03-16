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
#include "boost/filesystem.hpp"
#include <boost/regex.hpp>
#include <stdlib.h>

//we use the generic shared object management classes.
#include "cclient.h"
#include "csharedobject.h"
#include "cobjectman.h"

#include "exception/cexception.h"

namespace paper
{

	using namespace std;
	using namespace boost;
	using namespace boost::posix_time;
	using boost::filesystem::ofstream;
	using boost::filesystem::ifstream;


 	struct drand48_data gRandomBuffer;
	bool gShutdown;
	//most recent changed paper list
	synapse::Cconfig gPaperIndex;

	SYNAPSE_REGISTER(module_Init)
	{

		Cmsg out;

		gShutdown=false;
		srand48_r(time(NULL), &gRandomBuffer);
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
		out["event"]=	"paper_Join";		out.send(); //join a object
		out["event"]=	"paper_Leave";		out.send(); //leave a object
		out["event"]=	"paper_GetPapers";	out.send(); //a list of all papers (returns object_Object for every object)
		out["event"]=	"paper_GetClients";	out.send(); //a list of clients that are member of specified object. (returns object_Clients for every object)

		out["event"]=	"paper_ClientDraw";		out.send(); //draw something

		out["event"]=	"paper_Export";			out.send(); //export the paper to svg/png

		out["event"]=	"paper_GetList";			out.send(); //get a list of papers

		out["event"]=	"paper_Authenticate";			out.send();
		out["event"]=	"paper_ChangeAuth";			out.send();


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

	//TODO: move this to a generic lib
	//replace a bunch of regular expressions in a file.
	//the regex Cvar is a hasharray:
	// regex => replacement
	void regexReplaceFile(const string & inFilename, const string & outFilename,  synapse::CvarMap & regex)
	{
		//read input
		stringbuf inBuf;
		ifstream inStream;
		inStream.exceptions (  ifstream::failbit| ifstream::badbit );
		inStream.open(inFilename);
		inStream.get(inBuf,'\0');
		inStream.close();

		//build regex and formatter
		string regexStr;
		stringstream formatStr;
		int count=1;
		for(synapse::CvarMap::iterator I=regex.begin(); I!=regex.end(); I++)
		{
			if (count>1)
				regexStr+="|";

			regexStr+="("+ I->first +")";
			formatStr << "(?{" << count << "}" << I->second.str() << ")";
			count++;
		}

		//apply regexs
		string outBuf;
		outBuf=regex_replace(
				inBuf.str(),
				boost::regex(regexStr),
				formatStr.str(),
				boost::match_default | boost::format_all
		);

		//write output
		ofstream outStream;
		outStream.exceptions ( ofstream::eofbit | ofstream::failbit | ofstream::badbit );
		outStream.open(outFilename);
		outStream << outBuf;
		outStream.close();
	}

	//return a random readable string, for use as key or uniq id
	string randomStr(int length)
	{
		long int r;
		string chars("abcdefghijklmnopqrstuvwxyz0123456789");
		string s;
		for (int i=0; i<length; i++)
		{
			mrand48_r(&gRandomBuffer, &r);
			s=s+chars[abs(r) % chars.length()];
		}
		return(s);
	}




	//a client of a paper object
	class CpaperClient : public synapse::Cclient
	{
		friend class CpaperObject;
		private:

		public:
		Cvar mCursor;
		int mLastElementId;


		//authorized functions
		bool mAuthCursor;
		bool mAuthChat;
		bool mAuthView;
		bool mAuthChange;
		bool mAuthOwner;



		CpaperClient()
		{
			mLastElementId=0;
			mAuthView=false;
			mAuthChange=false;
			mAuthOwner=false;
			mAuthCursor=false;
			mAuthChat=false;

		}


		//sends a message to the client, only if the client has view-rights
		//otherwise the message is ignored.
		void sendFiltered(Cmsg & msg)
		{
			if (mAuthView)
				msg.send();

		}

		//authorizes the client withclient with key and rights
		void authorize(Cvar & rights)
		{
			mAuthView=rights["view"];
			mAuthChange=rights["change"];
			mAuthOwner=rights["owner"];
			mAuthCursor=rights["cursor"];
			mAuthChat=rights["chat"];

			//inform the client of its new rights
			Cmsg out;
			out.event="paper_Authorized";
			out.dst=id;
			out.map()=rights;
			out.send();

		}
	};


	//a server side piece of paper
	class CpaperObject : public synapse::CsharedObject<CpaperClient>
	{
		private:
		bool mExporting;
		synapse::Cconfig mDrawing;

		//called when the drawing-data of the drawing is modified. (e.g. new export is needed)

		void changed()
		{
			mDrawing["changeTime"]=time(NULL);
			mDrawing["exported"]=false;
			mDrawing["version"]=mDrawing["version"]+1;
			mDrawing.changed();
		}


		public:
		CpaperObject()
		{
			mExporting=false;
		}


		//send message to all clients that are joined, using filtering.
		void sendAllFiltered(Cmsg & msg)
		{
			CclientMap::iterator I;
			for (I=clientMap.begin(); I!=clientMap.end(); I++)
			{
				msg.dst=I->first;
				try
				{
					I->second.sendFiltered(msg);
				}
				catch(...)
				{
					; //expected raceconditions do occur during session ending, so ignore send errors
				}
			}
		}

		//get filenames, relative to wwwdir, or relative to synapse main dir.
		//these probably are going to give different results when papers are made private.
		string getSvgFilename(bool www=false)
		{
			stringstream filename;
			if (!www)
				filename << "wwwdir";
			filename << mDrawing["path"].str() << "paper.svg";
			//cache breaker
			if (www)
				filename << "?" << mDrawing["version"];

			return (filename.str());
		}

		string getPngFilename(bool www=false)
		{
			stringstream filename;
			if (!www)
				filename << "wwwdir";
			filename << mDrawing["path"].str()  << "paper.png";
			//cache breaker
			if (www)
				filename << "?" << mDrawing["version"];

			return (filename.str());
		}

		string getThumbFilename(bool www=false)
		{
			stringstream filename;
			if (!www)
				filename << "wwwdir";
			filename << mDrawing["path"].str() << "thumb.png";
			//cache breaker
			if (www)
				filename << "?" << mDrawing["version"];

			return (filename.str());
		}

		string getHtmlFilename(bool www=false)
		{
			stringstream filename;
			if (!www)
				filename << "wwwdir";
			filename << mDrawing["path"].str() << "edit.html";

			return (filename.str());
		}

		void createHtml()
		{
			//Since we need to add all kinds of metadata to the paper-html file, we need to parse the html file and fill in some marcros
			synapse::CvarMap regex;
			regex["%id%"]=id;
			regex["%png%"]=getPngFilename(true);
			regex["%thumb%"]=getThumbFilename(true);
			regex["%svg%"]=getSvgFilename(true);

			regexReplaceFile("wwwdir/paper/edit.html", getHtmlFilename(), regex);

		}

		//the paper is created for the first time.
		void create()
		{
			//1000r will be the root svg element with its settings
			//NOTE: svgweb doesnt support a numeric svg-root, hence the added r
			//NOTE: This is a STL ordered MAP, we need to keep the correct order, so hence the 1000.
			mDrawing["data"]["1000r"]["element"]="svg";
			mDrawing["data"]["1000r"]["version"]="1.2";
			mDrawing["data"]["1000r"]["baseProfile"]="tiny";
			mDrawing["data"]["1000r"]["viewBox"]="0 0 17777 10000";

			mDrawing["data"]["1000r"]["xmlns"]="http://www.w3.org/2000/svg";
			mDrawing["data"]["1000r"]["xmlns:xlink"]="http://www.w3.org/1999/xlink";
			//we dont use this YET:
			//drawing["data"]["1000r"]["xmlns:ev"]="http://www.w3.org/2001/xml-events";

			mDrawing["data"]["1000r"]["stroke-linecap"]="round";
			mDrawing["data"]["1000r"]["stroke-linejoin"]="round";

			//drawing.setAttribute("preserveAspectRatio", "none");
//				drawing.setAttribute("pointer-events","all");
//				drawing.setAttribute("color-rendering","optimizeSpeed");
//				drawing.setAttribute("shape-rendering","optimizeSpeed");
//				drawing.setAttribute("text-rendering","optimizeSpeed");
//				drawing.setAttribute("image-rendering","optimizeSpeed");

			//we start at 1001 so the order stays correct in the stl map. once we get to 10000 the order gets screwed up, but that probably never happens ;)
			mDrawing["lastElementId"]=1000;

			//create appropriate files
			mDrawing["path"]="/p/"+randomStr(8)+"/";
			filesystem::create_directory("wwwdir" + mDrawing["path"].str());
			createHtml();


		}


		//called by the object manager to get interesting metadata about this object
		void getInfo(Cvar & var)
		{
			synapse::CsharedObject<CpaperClient>::getInfo(var);
			var["changeTime"]=mDrawing["changeTime"];
			var["clients"]=clientMap.size();
			var["htmlPath"]=getHtmlFilename(true);
			var["thumbPath"]=getThumbFilename(true);
		}


		//called when exporting is done
		void exported()
		{
			mExporting=false;
			if (!mDrawing["exported"])
			{
				mDrawing["exported"]=true;
				mDrawing["hasThumb"]=true;
				mDrawing.changed();

				//update the recent-changed list?
				if (mDrawing["version"]>0)
				{
					//delete ourself from the list first
					for (synapse::CvarList::iterator paperI=gPaperIndex["list"].list().begin(); paperI!=gPaperIndex["list"].list().end(); paperI++)
					{
						if ((*paperI)["objectId"]==id)
						{
							gPaperIndex["list"].list().erase(paperI);
							break;
						}
					}

					//now re-add the updated version of ourself in the front position
					Cvar paperInfo;
					getInfo(paperInfo);
					gPaperIndex["list"].list().push_front(paperInfo);

					//never more then this amount in the index
					if (gPaperIndex["list"].list().size()>100)
						gPaperIndex["list"].list().pop_back();

					gPaperIndex.changed();
				}

			}

			//inform clients the export is ready
			Cmsg out;
			out.event="paper_Exported";
			out["svgPath"]=getSvgFilename(true);
			out["pngPath"]=getPngFilename(true);
			out["thumbPath"]=getThumbFilename(true);
			sendAllFiltered(out);

		}

		//export the drawing to svg and png files.
		//sends various a paper_Exported event when done.
		void saveExport(bool send=true)
		{
			//already exporting, dont start it again
			if (mExporting)
				return;

			//its already exported, dont do anything but send out the event.
			if (mDrawing["exported"])
			{
				if (send)
					exported();
			}
			else
			//export the drawing
			{
				mExporting=true;

				//export to svg
				ofstream svgStream;
				svgStream.exceptions ( ofstream::eofbit | ofstream::failbit | ofstream::badbit );
				svgStream.open(getSvgFilename());

				//svg header
				svgStream << "<?xml version=\"1.0\" standalone=\"no\"?>\n";
				svgStream << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n\n";

				//export all elements
				for(Cvar::iterator elementI=mDrawing["data"].begin(); elementI!=mDrawing["data"].end(); elementI++)
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
				Cmsg out;
				out.src=0;
				out.dst=0;
				out.event="exec_Start";
				out["id"]["paperId"]=id;
//				out["id"]["path"]=getPngFilename(true);
//				out["id"]["type"]="png";
				out["queue"]=1;
				out["cmd"]="nice convert -density 4 MSVG:" + getSvgFilename() + " " + getPngFilename() + " "+
						"&& nice convert -scale 150 " + getPngFilename() + " " + getThumbFilename();
				out.send();
			}
		}

		void execEnded(Cvar & var)
		{
			//for now executing is only for exporting (to be changed)
			exported();
		}


		void execError(Cvar & var)
		{
			execEnded(var);
		}

		void save(string path)
		{
			mDrawing.save(path);
			saveExport(false);
		}

		void load(string path)
		{
			mDrawing.load(path);
		}

		//authenticates clientId with key
		void authenticate(int clientId, string key)
		{
			//key doesnt exists?
			if (!mDrawing["auth"].isSet(key))
			{
				Cmsg out;
				out.event="paper_AuthWrongKey";
				out.dst=clientId;
				out.send();
				return;
			}

			//if the key exists, authorize the client
			getClient(clientId).authorize(mDrawing["auth"][key]);
		}

		//changes authentication keys and authorization
		//rights is just a hasharray
		//Specifying clientId checks if the client is allowed to give these rights
		//specifying an empty rights-array will delete the key
		void changeAuth(int clientId, string key, Cvar & rights)
		{
			if (!getClient(clientId).mAuthOwner)
				throw(synapse::runtime_error("Only the owner can change the security settings of this drawing."));

			mDrawing["auth"][key]["view"]=rights["view"];
			mDrawing["auth"][key]["change"]=rights["change"];
			mDrawing["auth"][key]["owner"]=rights["owner"];
			mDrawing["auth"][key]["cursor"]=rights["cursor"];
			mDrawing["auth"][key]["chat"]=rights["chat"];
			if (rights.isSet("description"))
				mDrawing["auth"][key]["description"]=rights["description"];

			mDrawing.changed();

		}

		//get all authorisation and authentication info
		void getAuth(int clientId)
		{
			if (!getClient(clientId).mAuthOwner)
				throw(synapse::runtime_error("Only the owner can get the security settings of this drawing."));


			throw(synapse::runtime_error("STUB"));
		}

		//send the drawing commands to all clients, except clientId
		//(use clientId 0 if your want the message send to all clients)
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
						I->second.sendFiltered(out);
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
			elementI=mDrawing["data"].map().find(id);
			if (elementI==mDrawing["data"].map().end())
				throw(synapse::runtime_error("Specified element id not found."));
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
			if (elementI!=mDrawing["data"].end())
			{
				//yes, so fill in beforeId,  so the objects stay in the correct order
				msg["beforeId"]=elementI->first;
			}
		}

		//resend all data to dst
		void reload(int dst)
		{
			if (!getClient(dst).mAuthView)
				throw(synapse::runtime_error("You're not authorized to view this drawing."));

			Cmsg out;
			out.event="paper_ServerDraw";
			out.dst=dst;

			//indicate start of complete reload
			out["cmd"]="reload";
			out.send();

			//send all elements
			for(Cvar::iterator elementI=mDrawing["data"].begin(); elementI!=mDrawing["data"].end(); elementI++)
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
				out["cursor"].map()=I->second.mCursor;
				out["src"]=I->first;
				out.send();
			}

			//send chat log
			for(CvarList::iterator I=mDrawing["chat"].list().begin(); I!=mDrawing["chat"].list().end(); I++)
			{
				out.clear();
				out["chat"]=*I;
				out.send();
			}


			out.clear();
			out["cmd"]="ready";
			out.send();
		}

		//process drawing data received from a client store it, and relay it to other clients via serverDraw
		//if a client is not authorized to do certain stuff, an exception is thrown
		void clientDraw(Cmsg & msg)
		{
			msg["src"]=msg.src;

			//received cursor information?
			if (msg.isSet("cursor"))
			{
				if (!getClient(msg.src).mAuthCursor)
					throw(synapse::runtime_error("You're not allowed to send cursor updates"));

				//store the new fields in the clients cursor object:
				Cvar & cursor=getClient(msg.src).mCursor;
				for(Cvar::iterator I=msg["cursor"].begin(); I!=msg["cursor"].end(); I++)
				{
					cursor[I->first]=I->second.str();
				}
			}

			//received chat?
			if (msg.isSet("chat"))
			{
				if (!getClient(msg.src).mAuthChat)
					throw(synapse::runtime_error("You're not allowed to chat"));

				//store in chat log for this object
				mDrawing["chat"].list().push_back(msg["chat"]);
			}

			//received drawing commands?
			if (msg["cmd"].str()=="update")
			{

				if (!getClient(msg.src).mAuthChange)
					throw(synapse::runtime_error("You're not authorized to change this drawing."));

				//id specified?
				if (msg.isSet("id"))
				{
					//add new object?
					if (msg["id"].str()=="new")
					{
						//figure out a fresh new id
						mDrawing["lastElementId"]=mDrawing["lastElementId"]+1;

						//store the new id in the message
						msg["id"]=mDrawing["lastElementId"];

						//create new element
						mDrawing["data"][msg["id"]]["element"]=msg["element"].str();

					}

					//store the last id for this client
					getClient(msg.src).mLastElementId=msg["id"];
				}
				//id not specified, automaticly add the last used id from this client
				else
				{
					msg["id"]=getClient(msg.src).mLastElementId;
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

				changed();
			}
			//send refresh of requested element?
			else if (msg["cmd"].str()=="refresh")
			{
				//id not set? the use the last one that was used by this client
				if (!msg.isSet("id"))
				{
					if (getClient(msg.src).mLastElementId)
						msg["id"]=getClient(msg.src).mLastElementId;
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
				getClient(msg.src).sendFiltered(out);

				return;
			}
			//delete object?
			else if (msg["cmd"].str()=="delete")
			{
				if (!getClient(msg.src).mAuthChange)
					throw(synapse::runtime_error("You're not authorized to change this drawing."));

				//delete it
				Cvar::iterator elementI=getElement(msg["id"]);
				mDrawing["data"].map().erase(elementI);
				changed();

				getClient(msg.src).mLastElementId=0;
			}
			//request a reload of all the data
			else if (msg["cmd"].str()=="reload")
			{
				reload(msg.src);
				return;
			}

			//relay the command to other clients
			serverDraw(msg,msg.src);
		}

		virtual void addClient(int id)
		{
			//let the base class do its work:
			synapse::CsharedObject<CpaperClient>::addClient(id);
		}

		virtual bool isIdle()
		{
			return (!mExporting && synapse::CsharedObject<CpaperClient>::isIdle());

		}

	};



	synapse::CobjectMan<CpaperObject> gObjectMan("var/paper");


	/** Client wants new paper
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

		//give the creator temporary owner rights.
		//The client show add its own key and rights, otherwise the drawing will be inaccessible after leaving it.
		Cvar rights;
		rights["owner"]=1;
		gObjectMan.getObject(objectId).getClient(msg.src).authorize(rights);

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
		gObjectMan.leaveAll(msg.src); //remove this if you want clients to be able to join multiple objects
		gObjectMan.getObject(msg["objectId"]).addClient(msg.src);

	}

	/** Try to authenticate client with key.
	 * \param key The key to autenticate with.
	 *
	 * \par Replies \c paper_Authorized when key was ok.
	 * \par Replies \c paper_AuthWrongKey when key was not found.
	 */
	SYNAPSE_REGISTER(paper_Authenticate)
	{
		gObjectMan.getObjectByClient(msg.src).authenticate(msg.src,msg["key"]);
	}

	/** Change authentication and authorisation info
	 * \param key The key to change or add. Specify an empty key to set the default rights.
	 * \param rights Hasharray of rights to apply. Leave empty to delete the key.
	 * 		\param chat User can chat.
	 * 		\param view User can view the drawing.
	 * 		\param cursor User can send cursor updates (other people with view-rights see the cursor, so you can point at stufF)
	 * 		\param change User can change the drawing.
	 * 		\param owner User is owner and can change rights
	 * 		\param description Description of the key
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
			gObjectMan.getObject(msg["id"]["paperId"]).execEnded(msg["id"]);
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

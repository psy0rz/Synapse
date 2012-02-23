#include "cpaperobject.h"
#include "utils.h"
//#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/fstream.hpp"
//#include "boost/filesystem.hpp"


namespace paper
{
	using namespace boost;
//	using namespace boost::posix_time;
	using boost::filesystem::ofstream;
//	using boost::filesystem::ifstream;

	//called when the drawing-data of the drawing is modified. (e.g. new export is needed)

	void CpaperObject::changed()
	{
		mDrawing["changeTime"]=time(NULL);
		mDrawing["exported"]=false;
		mDrawing["version"]=mDrawing["version"]+1;
		mDrawing.changed();
	}


	CpaperObject::CpaperObject()
	{
		mExporting=false;
	}


	//send message to all clients that are joined, using filtering.
	void CpaperObject::sendAll(Cmsg & msg)
	{
		CclientMap::iterator I;
		for (I=clientMap.begin(); I!=clientMap.end(); I++)
		{
			msg.dst=I->first;
			try
			{
				msg.send();
			}
			catch(...)
			{
				; //expected raceconditions do occur during session ending, so ignore send errors
			}
		}
	}

	//get filenames, relative to wwwdir, or relative to synapse main dir.
	//these probably are going to give different results when papers are made private.
	string CpaperObject::getSvgFilename(bool www)
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

	string CpaperObject::getPngFilename(bool www)
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

	string CpaperObject::getThumbFilename(bool www)
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

	string CpaperObject::getHtmlFilename(bool www)
	{
		stringstream filename;
		if (!www)
			filename << "wwwdir";
		filename << mDrawing["path"].str() << "edit.html";

		return (filename.str());
	}

	void CpaperObject::createHtml()
	{
		//Since we need to add all kinds of metadata to the paper-html file, we need to parse the html file and fill in some marcros
		synapse::CvarMap regex;
		regex["%id%"]=id;
		regex["%png%"]=getPngFilename(true);
		regex["%thumb%"]=getThumbFilename(true);
		regex["%svg%"]=getSvgFilename(true);

		utils::regexReplaceFile("wwwdir/paper/edit.html", getHtmlFilename(), regex);

	}

	//the paper is created for the first time.
	void CpaperObject::create()
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
		mDrawing["path"]="/p/"+utils::randomStr(8)+"/";
		filesystem::create_directory("wwwdir" + mDrawing["path"].str());
		createHtml();


	}


	/** Fills var with information about the drawing.
	 * \P changeTime Time of last change.
	 * \P clients Number of joined clients
	 * \P htmlPath Path to html file to edit drawing.
	 * \P thumbPath Path to thumbnail of drawing.
	 * \P version Version of the drawing (increases with every change)
	 *
	 */
	void CpaperObject::getInfo(Cvar & var)
	{
		synapse::CsharedObject<CpaperClient>::getInfo(var);
		var["changeTime"]=mDrawing["changeTime"];
		var["clients"]=clientMap.size();
		var["htmlPath"]=getHtmlFilename(true);
		var["thumbPath"]=getThumbFilename(true);
		var["version"]=mDrawing["version"];
	}


	//called when exporting is done
	void CpaperObject::exported()
	{
		mExporting=false;
		if (!mDrawing["exported"])
		{
			mDrawing["exported"]=true;
			mDrawing["hasThumb"]=true;
			mDrawing.changed();


		}

		//inform clients the export is ready
		Cmsg out;
		out.event="paper_Exported";
		out["svgPath"]=getSvgFilename(true);
		out["pngPath"]=getPngFilename(true);
		out["thumbPath"]=getThumbFilename(true);
		sendAll(out);

	}

	//export the drawing to svg and png files.
	//sends various a paper_Exported event when done.
	void CpaperObject::saveExport(bool send)
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

	void CpaperObject::execEnded(Cvar & var)
	{
		//for now executing is only for exporting (to be changed)
		exported();
	}


	void CpaperObject::execError(Cvar & var)
	{
		execEnded(var);
	}

	void CpaperObject::save(string path)
	{
		mDrawing.save(path);
		saveExport(false);
	}

	void CpaperObject::load(string path)
	{
		mDrawing.load(path);
	}

	//authenticates clientId with key
	void CpaperObject::login(int clientId, string key)
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
		//if the key exists, add and authorize the client
		synapse::CsharedObject<CpaperClient>::addClient(clientId);
		getClient(clientId).authorize(mDrawing["auth"][key]);

		//inform all clients about the new credentials
		sendClientUpdate(clientId);
	}

	//changes authentication keys and authorization
	//rights is just a hasharray
	//Specifying clientId checks if the client is allowed to give these rights
	//specifying an empty rights-array will delete the key
	void CpaperObject::changeAuth(int clientId, string key, Cvar & rights)
	{
		if (!getClient(clientId).mAuthOwner)
			throw(synapse::runtime_error("Only the owner can change the security settings of this drawing."));

		if (rights.map().size()!=0)
		{
			mDrawing["auth"][key]["view"]=rights["view"];
			mDrawing["auth"][key]["change"]=rights["change"];
			mDrawing["auth"][key]["owner"]=rights["owner"];
			mDrawing["auth"][key]["cursor"]=rights["cursor"];
			mDrawing["auth"][key]["chat"]=rights["chat"];
			if (rights.isSet("description"))
				mDrawing["auth"][key]["description"]=rights["description"];
		}
		else
		{
			mDrawing["auth"].map().erase(key);
		}

		mDrawing.changed();

	}

	//get all authorisation and authentication info
	void CpaperObject::getAuth(int clientId)
	{
		if (!getClient(clientId).mAuthOwner)
			throw(synapse::runtime_error("Only the owner can get the security settings of this drawing."));


		throw(synapse::runtime_error("STUB"));
	}

	//send the drawing commands to all clients, except clientId
	//(use clientId 0 if your want the message send to all clients)
	void CpaperObject::serverDraw(Cmsg out, int clientId )
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
	Cvar::iterator CpaperObject::getElement(const string & id)
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
	void CpaperObject::element2msg(const string & id, Cmsg & msg)
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
	void CpaperObject::reload(int dst)
	{
//		if (!getClient(dst).mAuthView)
//			throw(synapse::runtime_error("You're not authorized to view this drawing."));

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
	void CpaperObject::clientDraw(Cmsg & msg)
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
			out.send();

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

//	void CpaperObject::addClient(int id)
//	{
//		//let the base class do its work:
//		synapse::CsharedObject<CpaperClient>::addClient(id);
//	}

	bool CpaperObject::isIdle()
	{
		return (!mExporting && synapse::CsharedObject<CpaperClient>::isIdle());

	}
}

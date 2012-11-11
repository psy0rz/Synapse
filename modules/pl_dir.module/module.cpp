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
The play list module.

This module can dynamicly generate playlists from directory's. It also can cache per path metadata.



*/


#include "synapse.h"
#include "cconfig.h"
#include <string.h>
#include <errno.h>
#include <map>
#include <boost/shared_ptr.hpp>

#include "exception/cexception.h"

#define BOOST_FILESYSTEM_VERSION 3
#include "boost/filesystem.hpp"


/** Playlist namespace
 *
 */
namespace pl
{
	using namespace std;
	using namespace boost::filesystem;
	using namespace boost;

	bool shutdown;
	int defaultId=-1;

	SYNAPSE_REGISTER(module_Init)
	{
		Cmsg out;
		shutdown=false;
		defaultId=msg.dst;

		//load config file
		synapse::Cconfig config;
		config.load("etc/synapse/pl.conf");

		out.clear();
		out.event="core_ChangeModule";
		out["maxThreads"]=1;
		out.send();

		out.clear();
		out.event="core_ChangeSession";
		out["maxThreads"]=1;
		out.send();

		out.clear();
		out.event="core_LoadModule";
		out["path"]="modules/http_json.module/libhttp_json.so";
		out.send();


		//tell the rest of the world we are ready for duty
		out.clear();
		out.event="core_Ready";
		out.send();

	}

	SYNAPSE_REGISTER(module_SessionStart)
	{
	}



	class Cpath : public path
	{
		private:
		time_t mWriteDate;

		public:

		Cpath(const path & p)
		:path(p)
		{
			mWriteDate=0;
		}


		//get/cache modification date
		int getDate()
		{
			if (!mWriteDate)
				mWriteDate=last_write_time(*this);

			return (mWriteDate);
		}

		//get sort filename string
		std::string getSortName()
		{
			return(filename().string());
		}



		//get/cache metadata field (from the path database)
		std::string getMeta(std::string key)
		{
			throw(synapse::runtime_error("not implemented yet!"));
			return("error");
		}

		void setMeta(std::string key, std::string value)
		{
			throw(synapse::runtime_error("not implemented yet!"));

		}
	};

	class CsortedDir: public list<Cpath>
	{
		private:
		path mBasePath;
		string mSortField;

		public:
		enum Efiletype  { FILE, DIR, ALL };
//		list<Cpath> mPaths;
//		list<Cpath>::iterator iterator;

		static bool compareFilename (Cpath first, Cpath second)
		{
			return (first.getSortName() < second.getSortName());
		}

		static bool compareDate (Cpath first, Cpath second)
		{
			return (first.getDate() < second.getDate());
		}


		CsortedDir(path basePath, string sortField, Efiletype filetype)
		{
			mBasePath=basePath;
			mSortField=sortField;

			DEB("Reading directory " << basePath.string());
			directory_iterator end_itr;
			for ( directory_iterator itr( basePath );
				itr != end_itr;
				++itr )
			{
				path p;
				p=itr->path().filename();
				if (
						(filetype==ALL) ||
						(filetype==DIR && is_directory(*itr)) ||
						(filetype==FILE && is_regular(*itr))
				)
				{
					push_back(p);
				}
			}

			if (sortField=="filename")
				sort(compareFilename);
			else if (sortField=="date")
				sort(compareDate);
			else
				throw(synapse::runtime_error("sort mode not implemented yet!"));
		}

	};


	class Citer
	{
		private:
		path mRootPath;
		path mCurrentPath;
		path mCurrentFile;


		string mId;


		private:

		//to make stuff more readable and less error prone
		enum Edirection { NEXT, PREVIOUS };
		enum Erecursion { RECURSE, DONT_RECURSE };

		/*
		/a/b
		/a/b/c/d
		*/

		//traverses directories/files
		//currentPath contains the 'selected' path. direction tells if you want to move up or down.
		//returns resulting path after this movement. when first or last path is reached it loops.
		//recursion means, recurse until we're at a file
		//rootPath is the highest path, it can never be escaped.
		path movePath(path rootPath, path currentPath, string sortField, Edirection direction, Erecursion recursion, CsortedDir::Efiletype filetype)
		{
//			DEB("rootpath:" << rootPath);
//			DEB("currentpath:" << currentPath);
			
			//determine the path we should get the initial listing of:
			path listPath;
			if (currentPath==rootPath)
				listPath=currentPath;
			else
				listPath=currentPath.parent_path();

			path startPath=currentPath;
			
			CsortedDir::iterator dirI;
			do
			{
				//get sorted directory listing
				CsortedDir sortedDir(listPath, sortField, filetype);

				if (!sortedDir.empty())
				{
					
					//try to find the current path:
					if (!currentPath.empty())
						dirI=find(sortedDir.begin(), sortedDir.end(), currentPath.filename());
					else
						dirI=sortedDir.end();

					//currentPath not found?
					if (dirI==sortedDir.end())
					{
						//start at the first or last entry depending on direction
						if (direction==NEXT)
							dirI=sortedDir.begin();
						else
						{
							dirI=sortedDir.end();
							dirI--;
						}
					}
					else
					{
						//move one step in the correct direction
						if (direction==NEXT)
						{
							dirI++;
						}
						//PREVIOUS:
						else
						{
							if (dirI==sortedDir.begin())
								dirI=sortedDir.end();
							else
								dirI--;
						}
					}

					//top or bottom was reached
					if (dirI==sortedDir.end())
					{
						//can we one dir higher?
						if (recursion==RECURSE && listPath!=rootPath)
						{
							//yes, so go one dir higher and continue the loop
							currentPath=listPath;
							listPath=currentPath.parent_path();
						}
						else
						{
							//no, cant go higher.
							//clear the current path, so it just gets the first or last entry
							currentPath.clear();
						}
					}
					//we found something
					else
					{
						//should we recurse?
						if (recursion==RECURSE && is_directory(listPath/(*dirI)))
						{
							//enter it
							listPath=listPath/(*dirI);
							currentPath.clear();
						}
						else
						{
							//return the new path
							return (listPath/(*dirI));
						}
					}
				}
				else
				{
					//list is empty, our last chance is to go one dir higher, otherwise we will exit the loop:
					if (recursion==RECURSE && listPath!=rootPath)
					{
						//go one dir higher and continue the loop
						currentPath=listPath;
						listPath=currentPath.parent_path();
					}
				}

			}
			while(currentPath!=startPath);

			//nothing found, just return currentPath
			return(currentPath);
		}

		public:

		//next file
		void next()
		{
			mCurrentFile=movePath(mCurrentPath, mCurrentFile, "filename", NEXT, RECURSE, CsortedDir::ALL);
		}

		//prev file
		void previous()
		{
			mCurrentFile=movePath(mCurrentPath, mCurrentFile,"filename", PREVIOUS, RECURSE, CsortedDir::ALL);
		}

		void nextDir()
		{
			mCurrentPath=movePath(mRootPath, mCurrentPath, "filename", NEXT, DONT_RECURSE, CsortedDir::DIR);
			mCurrentFile=mCurrentPath;
			next();
		}

		void previousDir()
		{
			mCurrentPath=movePath(mRootPath, mCurrentPath, "filename", PREVIOUS, DONT_RECURSE, CsortedDir::DIR);
			mCurrentFile=mCurrentPath;
			previous();
		}

		void exitDir()
		{
			if (mCurrentPath!=mRootPath)
			{
				mCurrentPath=mCurrentPath.parent_path();
				mCurrentFile=mCurrentPath;
				next();
			}
		}

		void enterDir()
		{
			//find the first directory in that directory, and dont go higher:
			mCurrentPath=movePath(mCurrentPath, mCurrentPath, "filename", NEXT, DONT_RECURSE, CsortedDir::DIR);
			mCurrentFile=mCurrentPath;
			next();
		}

		void reset()
		{
			mCurrentPath=mRootPath;
			mCurrentFile=mRootPath;
			next();
		}

		void create(string id, string rootPath)
		{
			mId=id;
			mRootPath=rootPath;
			DEB("Created iterator " << id << " for path " << rootPath);
			reset();
		}


		void send(int dst)
		{
			Cmsg out;
			out.event="pl_Entry";
			out.dst=dst;
			out["id"]=mId;
			out["rootPath"]=mRootPath.string();
			out["currentPath"]=mCurrentPath.string();
			out["currentFile"]=mCurrentFile.string();

//			if (*mIterDir!= directory_iterator())
//				out["selectedDir"]=(*mIterDir)->path().directory_string();
//
//			if (*mIterFile!= recursive_directory_iterator())
//				out["selectedFile"]=(*mIterFile)->path().file_string();

			out.send();
		}

		void destroy()
		{

		}

	};

	class CiterMan
	{
		private:
		typedef map<string,Citer> CiterMap;
		CiterMap mIterMap;

		public:

		Citer & get(string id)
		{
			if (mIterMap.find(id)==mIterMap.end())
				throw(synapse::runtime_error("Playlist not found"));
			return(mIterMap[id]);
		}

		void create(string id, string basePath)
		{
			if (mIterMap.find(id)!=mIterMap.end())
				throw(synapse::runtime_error("Playlist already exists"));

			mIterMap[id].create(id, basePath);
		}

		void destroy(string id)
		{
			get(id).destroy();
			mIterMap.erase(id);
		}


	};

	CiterMan iterMan;


	/** Create a new iterator
		\param id Traverser id
		\path path Base path. Iterator can never 'escape' this directory.

		SECURITY WARNING: Its possible to traverse the whole filesystem for users that have permission to send pl_Create!

	\par Replies pl_Entry:
		\param id Traverser id
		\param path Current path
		\param file Current file, selected according to search criteria

	*/
	SYNAPSE_REGISTER(pl_Create)
	{
		iterMan.create(msg["id"], msg["path"]);
		iterMan.get(msg["id"]).send(msg.src);
	}


	/** Delete specified iterator
		\param id Traverser id
	*/
	SYNAPSE_REGISTER(pl_Destroy)
	{
		iterMan.destroy(msg["id"]);
	}


	/** Change selection/search criteria for files. Initalise a new iterator
		\param id Traverser id
		\param recurse Recurse level (-1 is infinite depth)
		\param fileOrder Order in which to traverse files (date, random, name)
		\param dirOrder  Order in which to directorys files (date, name)
		\param search	Search parameters for metadata or filename (details later!)

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_Mode)
	{

	}

	/** Get current directory and file
		\param id Traverser id

	\par Replies pl_Entry.
	*/
	SYNAPSE_REGISTER(pl_Current)
	{
		iterMan.get(msg["id"]).send(msg.src);
	}

	/** Select next directory entry in list
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_NextDir)
	{
		iterMan.get(msg["id"]).nextDir();
		iterMan.get(msg["id"]).send(msg.src);

	}


	/** Select previous entry directory in list
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_PreviousDir)
	{
		iterMan.get(msg["id"]).previousDir();
		iterMan.get(msg["id"]).send(msg.src);
	}

	/** Enters selected directory
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_EnterDir)
	{
		iterMan.get(msg["id"]).enterDir();
		iterMan.get(msg["id"]).send(msg.src);
	}

	/** Exits directory, selecting directory on higher up the hierarchy
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_ExitDir)
	{
		iterMan.get(msg["id"]).exitDir();
		iterMan.get(msg["id"]).send(msg.src);

	}

	/** Next song
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_Next)
	{
		iterMan.get(msg["id"]).next();
		iterMan.get(msg["id"]).send(msg.src);

	}

	/** Previous song
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_Previous)
	{
		iterMan.get(msg["id"]).previous();
		iterMan.get(msg["id"]).send(msg.src);
	}




	SYNAPSE_REGISTER(module_Shutdown)
	{
		shutdown=true;
	}

}

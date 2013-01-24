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

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

/** Playlist namespace
 *
 */
namespace pl
{
	using namespace std;
	using namespace boost::filesystem;
	using namespace boost;



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
		string mSortField;

		public:
        path mBasePath;
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
		}

	};


    //compares two absolute paths, to see if subdir is really a subdir of dir. (or the same dir)
    bool isSubdir(path dir, path subdir)
    {
        if (dir==subdir)
        {
            return(true);
        }

        while (!subdir.empty())
        {
            subdir=subdir.parent_path();
            if (dir==subdir)
                return(true);
        }
        return(false);
    }


    /*
    /a/b
    /a/b/c/d
    */

    /*** traverses directories/files

    -currentPath contains the currently 'selected' path.
    -rootPath is the 'highest' path we can ever reach.
    -filetype determines if we're looking for a file or directory or both.
    -direction tells if you want the next or previous file or directory.
    -RECURSE: its allow to enter or exit directories to find the next of previous file.

    returns resulting path after this movement.
    when first or last path is reached it loops.

    */
    enum Edirection { NEXT, PREVIOUS };
    enum Erecursion { RECURSE, DONT_RECURSE };
    path movePath(path rootPath, path currentPath, string sortField, Edirection direction, Erecursion recursion, CsortedDir::Efiletype filetype, int steps=1)
    {
        DEB(
            "currentPath=" << currentPath.string() <<
            " sortField=" << sortField <<
            " direction=" << direction <<
            " recursion=" << recursion << 
            " filetype=" << filetype <<
            " steps=" << steps);

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

                //as long as the listPath doesnt change, there is no need to reread the dir.
                //(this is neccesary to make steps>1 more efficient)
                while (listPath==sortedDir.mBasePath)
                {
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
                        //make a step in the right direction
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
                            listPath=listPath.parent_path();
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
                            //we found it
                            steps--;
                            if (steps<=0)
                            {
                                DEB("found, returning " << listPath/(*dirI));
                                return (listPath/(*dirI));
                            }
                            currentPath.clear();
                            //go on until steps is 0...
                        }
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
                    listPath=listPath.parent_path();
                }
            }

        }
        while(steps>0 || listPath!=startPath); //prevent inifinte loops if we dont find anything

        DEB("nothing found, returning " << startPath)
        return(startPath);
    }

	class Citer
	{
		private:
		path mRootPath;
		path mCurrentPath;
		path mCurrentFile;

        list<path> mPrevFiles;
        list<path> mNextFiles;
        list<path> mPrevPaths;
        list<path> mNextPaths;

		int mId;

        unsigned int mNextLen;
        unsigned int mPrevLen;

        int mRandomSteps; //1 is no randomisation and skipping
        boost::random::mt19937 mRandomGenerator;
        string mSortField;

        //this returns a random nummer between 1 and mRandomSteps
        int randomSteps()
        {
            if (mRandomSteps==1)
                return(1);
            else
            {
                boost::random::uniform_int_distribution<> dist(50, mRandomSteps+50);
                return dist(mRandomGenerator);
            }
        }
	
        //make sure there are enough entries in the file and path lists
        void updateLists()
        {
            //next file list:
            //make sure its not too long
            while (mNextFiles.size()>mNextLen)
                mNextFiles.pop_back();

            //fill the back with newer entries until its long enough
            {
                path p=mCurrentFile;
                if (!mNextFiles.empty())
                    p=mNextFiles.back();
                while(mNextFiles.size()<mNextLen)
                {
                    p=movePath(mCurrentPath, p, mSortField, NEXT, RECURSE, CsortedDir::ALL, randomSteps());
                    mNextFiles.push_back(p.string());
                }
            }

            //prev file list:
            //make sure its not too long
            while (mPrevFiles.size()>mPrevLen)
                mPrevFiles.pop_back();

            //fill the back older entries until its long enough
            {
                path p=mCurrentFile;
                if (!mPrevFiles.empty())
                    p=mPrevFiles.back();
                while(mPrevFiles.size()<mPrevLen)
                {
                    p=movePath(mCurrentPath, p, mSortField, PREVIOUS, RECURSE, CsortedDir::ALL, randomSteps());
                    mPrevFiles.push_back(p.string());
                }
            }

            //next path list:
            //make sure its not too long
            while (mNextPaths.size()>mNextLen)
                mNextPaths.pop_back();

            //fill the back with newer entries until its long enough
            {
                path p=mCurrentPath;
                if (!mNextPaths.empty())
                    p=mNextPaths.back();
                while(mNextPaths.size()<mNextLen)
                {
					p=movePath(mRootPath, p, mSortField, NEXT, DONT_RECURSE, CsortedDir::DIR);
                    mNextPaths.push_back(p.string());
                }
            }

            //prev path list:
            //make sure its not too long
            while (mPrevPaths.size()>mPrevLen)
                mPrevPaths.pop_back();

            //fill the back with newer entries until its long enough
            {
                path p=mCurrentPath;
                if (!mPrevPaths.empty())
                    p=mPrevPaths.back();
                while(mPrevPaths.size()<mPrevLen)
                {
					p=movePath(mRootPath, p, mSortField, PREVIOUS, DONT_RECURSE, CsortedDir::DIR);
                    mPrevPaths.push_back(p.string());
                }
            }
        }

		public:

        Citer()
        {
            mNextLen=5;
            mPrevLen=5;

            mRandomSteps=1;
            mSortField="filename";
        }

		//reload all file data. call this after you've changed currentPath or other settings
		void reloadFiles()
		{
			//clear lists
			mNextFiles.clear();
			mPrevFiles.clear();

			//current file doesnt belong to current path?
            if (!isSubdir(mCurrentPath, mCurrentFile))
            {
                //find the first valid file
                mCurrentFile=movePath(mCurrentPath, mCurrentPath, mSortField, NEXT, RECURSE, CsortedDir::ALL, randomSteps());
            } 

            //fill the lists
            updateLists();

		}

		//reload path and file data, call this after you exited or entered a path so that the previous/next paths need to be reloaded 
		void reloadPaths()
		{
			mNextPaths.clear();
			mPrevPaths.clear();
			reloadFiles();
		}

        void setMode(Cvar params)
        {
            if (params.isSet("randomSteps") && params["randomSteps"]>0 && params["randomSteps"]<10000)
                mRandomSteps=params["randomSteps"];

            if (params.isSet("sortField"))
                mSortField=params["sortField"].str();

            reloadPaths();
        }

		//next file
		void next()
		{
            mPrevFiles.push_front(mCurrentFile);
            mCurrentFile=mNextFiles.front();
            mNextFiles.pop_front();
            updateLists();
		}

		//prev file
		void previous()
		{
            mNextFiles.push_front(mCurrentFile);
            mCurrentFile=mPrevFiles.front();
            mPrevFiles.pop_front();
            updateLists();
		}

		void nextPath()
		{
            mPrevPaths.push_front(mCurrentPath);
            mCurrentPath=mNextPaths.front();
            mNextPaths.pop_front();
			reloadFiles();
		}

		void previousPath()
		{
            mNextPaths.push_front(mCurrentPath);
            mCurrentPath=mPrevPaths.front();
            mPrevPaths.pop_front();
            reloadFiles();
		}

		void exitPath()
		{
			if (mCurrentPath!=mRootPath)
			{
				mCurrentPath=mCurrentPath.parent_path();
				reloadPaths();
			}
		}


        //go one directory deeper, using the currentFile as reference
		void enterPath()
		{
            path p;
            p=mCurrentFile;
            while (!p.empty())
            {
                if (p.parent_path()==mCurrentPath)
                {
                    if (is_directory(p))
                    {
                        mCurrentPath=p;
                        reloadPaths();
                        return;
                    }
                    //the currentfile doesnt have a directory, so just use the first subdir we can find
                    else
                    {
                        mCurrentPath=movePath(mCurrentPath, mCurrentPath, mSortField, NEXT, DONT_RECURSE, CsortedDir::DIR);
                        reloadPaths();
                        return;
                    }
                }
                p=p.parent_path();
            }
		}


		void create(int id, string rootPath)
		{
			mId=id;
			mRootPath=rootPath;

			//find first path
			mCurrentPath=movePath(mRootPath, mRootPath, mSortField, NEXT, DONT_RECURSE, CsortedDir::DIR);
			reloadPaths();


			DEB("Created iterator " << id << " for path " << rootPath);
			
		}


		void send(int dst)
		{
            //there are lots of places/situations where things can go wrong, so do this extra check:
            if (!isSubdir(mRootPath, mCurrentPath) || !isSubdir(mRootPath, mCurrentFile))
            {
                ERROR("escaped rootpath: " << mRootPath << " " << mCurrentPath << " " << mCurrentFile);
                throw(synapse::runtime_error("Program error: ended up outside rootpath. (dont use trailing slashes for rootpath)"));
            }

			Cmsg out;
			out.event="pl_Entry";
			out.dst=dst;
			out["rootPath"]=mRootPath.string();
			out["currentPath"]=mCurrentPath.string();
			out["currentFile"]=mCurrentFile.string();

            //to make life easier for user interfaces in a crossplatform way:
            out["parentPath"]=mCurrentPath.parent_path().string();

            BOOST_FOREACH(path prevFile, mPrevFiles)
            {
                out["prevFiles"].list().push_back(prevFile.string());
            }
    
            BOOST_FOREACH(path nextFile, mNextFiles)
            {
                out["nextFiles"].list().push_back(nextFile.string());
            }

            BOOST_FOREACH(path prevPath, mPrevPaths)
            {
                out["prevPaths"].list().push_back(prevPath.string());
            }
    
            BOOST_FOREACH(path nextPath, mNextPaths)
            {
                out["nextPaths"].list().push_back(nextPath.string());
            }

            out["randomSteps"]=mRandomSteps;
            out["sortField"]=mSortField;

			out.send();
		}

		void destroy()
		{

		}

	};

	class CiterMan
	{
		private:
		typedef map<int,Citer> CiterMap;
		CiterMap mIterMap;

		public:

		Citer & get(int id)
		{
			if (mIterMap.find(id)==mIterMap.end())
				throw(synapse::runtime_error("Playlist not found"));
			return(mIterMap[id]);
		}

		void create(int id, string basePath)
		{
			if (mIterMap.find(id)!=mIterMap.end())
				throw(synapse::runtime_error("Playlist already exists"));

			mIterMap[id].create(id, basePath);
		}

		void destroy(int id)
		{
			get(id).destroy();
			mIterMap.erase(id);
		}


	};

	CiterMan iterMan;
    synapse::Cconfig config;

    bool shutdown;
    int defaultId=-1;

    SYNAPSE_REGISTER(module_Init)
    {
        Cmsg out;
        shutdown=false;
        defaultId=msg.dst;

        //load config file
        config.load("etc/synapse/pl.conf");

        out.clear();
        out.event="core_ChangeModule";
        out["maxThreads"]=1;
        out.send();

        out.clear();
        out.event="core_ChangeSession";
        out["maxThreads"]=1;
        out.send();

        //tell the rest of the world we are ready for duty
        out.clear();
        out.event="core_Ready";
        out.send();

    }

    SYNAPSE_REGISTER(module_SessionStart)
    {
        if (msg.isSet("path"))
            iterMan.create(dst, msg["path"]);
        else
            iterMan.create(dst, config["path"]);

        iterMan.get(dst).send(0);
    }

    SYNAPSE_REGISTER(module_SessionEnd)
    {
        iterMan.destroy(dst);
    }

	/** Create a new iterator
		\param id Traverser id
		\path path Base path. Iterator can never 'escape' this directory.

		SECURITY WARNING: Its possible to traverse the whole filesystem for users that have permission to send pl_Create!

	\par Replies pl_Entry:
		\param path Current path
		\param file Current file, selected according to search criteria

	*/
	SYNAPSE_REGISTER(pl_New)
	{
        Cmsg out;
        out.event="core_NewSession";
        out["path"]=msg["path"];
        out.dst=1;
        out.send();
	}


	/** Delete specified iterator (actually ends session, so you cant destroy default iterator)
		\param id Traverser id
	*/
	SYNAPSE_REGISTER(pl_Del)
	{
        if (msg.dst!=defaultId)
		{
            Cmsg out;
            out.event="core_DelSession";
            out.dst=1;
            out.send();
        }
	}


	/** Change selection/search criteria for files. Initalise a new iterator
        \param s

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_SetMode)
	{
        iterMan.get(dst).setMode(msg);
        iterMan.get(dst).send(0);

	}

	/** Get current directory and file
		\param id Traverser id

	\par Replies pl_Entry.
	*/
	SYNAPSE_REGISTER(pl_GetStatus)
	{
		iterMan.get(dst).send(msg.src);
	}

	/** Select next directory entry in list
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_NextPath)
	{
		iterMan.get(dst).nextPath();
		iterMan.get(dst).send(0);

	}


	/** Select previous entry directory in list
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_PreviousPath)
	{
		iterMan.get(dst).previousPath();
		iterMan.get(dst).send(0);
	}

	/** Enters selected directory
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_EnterPath)
	{
		iterMan.get(dst).enterPath();
		iterMan.get(dst).send(0);
	}

	/** Exits directory, selecting directory on higher up the hierarchy
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_ExitPath)
	{
		iterMan.get(dst).exitPath();
		iterMan.get(dst).send(0);

	}

	/** Next song
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_Next)
	{
		iterMan.get(dst).next();
		iterMan.get(dst).send(0);

	}

	/** Previous song
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_Previous)
	{
		iterMan.get(dst).previous();
		iterMan.get(dst).send(0);
	}




	SYNAPSE_REGISTER(module_Shutdown)
	{
		shutdown=true;
	}

}

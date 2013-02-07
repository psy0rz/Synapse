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

#include <boost/date_time/posix_time/posix_time.hpp>
 
 /** Playlist namespace
 *
 */
namespace pl
{
	using namespace std;
	using namespace boost::filesystem;
	using namespace boost;
    using namespace boost::posix_time;



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
        Efiletype mFiletype;
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


		void read(path basePath, string sortField, Efiletype filetype)
		{
            //if nothing has changed, dont reread if its same dir
            if (mBasePath==basePath && sortField==mSortField && filetype==mFiletype)
                return;

			mBasePath=basePath;
			mSortField=sortField;
            mFiletype=filetype;
            clear();

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
    -recurse: its allow to enter or exit directories to find the next of previous file.
    -loop: loop around if we're at end. otherwise return empty path object.

    returns resulting path after this movement.
    when first or last path is reached it loops.

    */


    enum Edirection { NEXT, PREVIOUS };
    enum Erecursion { RECURSE, DONT_RECURSE };
    enum Eloop      { LOOP, DONT_LOOP };

    path movePath(path rootPath, path currentPath, string sortField, Edirection direction, Erecursion recursion, CsortedDir::Efiletype filetype, Eloop loop=LOOP)
    {
        DEB(" rootPath=" << rootPath.string() <<
             " currentPath=" << currentPath.string() <<
             " sortField=" << sortField <<
             " direction=" << direction <<
             " recursion=" << recursion << 
             " filetype=" << filetype << 
             " loop=" << loop);

        if (currentPath.empty())
        {
            DEB("currentpath empty, returning empty as well");
            return (path());
        }


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
            static CsortedDir sortedDir;
            sortedDir.read(listPath, sortField, filetype);

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
                        //clear the current path, so it just gets the first or last entry, when we're in loop mode
                        if (loop==LOOP)
                            currentPath.clear();
                        else
                        {
                            DEB("end reached, not looping, so returning empty path");
                            return(path());
                        }

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
                        DEB("found, returning " << listPath/(*dirI));
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
                    listPath=listPath.parent_path();
                }
            }

        }
        while(listPath!=startPath); //prevent inifinte loops if we dont find anything

        DEB("nothing found, returning empty path")
        return(path());
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

        boost::random::mt19937 mRandomGenerator;
        string mSortField;
        unsigned int mRandomLength; //max length of random queue, the more, the better it is, but takes more time scanning 
        path mLastRandomFile; //last queued random path


        public:
	
        //make sure there are enough entries in the file and path lists
        //when it returns true, it should be called again. (used for scanning in small increments to improve response time)
        bool updateLists()
        {
            bool needs_more=false;

            //normal mode:
            if (mRandomLength==0)
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
                        p=movePath(mCurrentPath, p, mSortField, NEXT, RECURSE, CsortedDir::ALL);
                        if (p.empty())
                            break;
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
                        p=movePath(mCurrentPath, p, mSortField, PREVIOUS, RECURSE, CsortedDir::ALL);
                        if (p.empty())
                            break;
                        mPrevFiles.push_back(p.string());
                    }
                }

            }
            //random mode
            else
            {
                //make sure prevlist is not too long
                while (mPrevFiles.size()>mRandomLength)
                    mPrevFiles.pop_back();


                //time to regenerate the random list?
                if (mNextFiles.empty())
                {
                    mLastRandomFile=mCurrentFile;

                    //if this is the last file in the diretory structure, then restart with the mCurrentPath
                    if (movePath(mCurrentPath, mLastRandomFile, mSortField, NEXT, RECURSE, CsortedDir::ALL, DONT_LOOP).empty())
                        mLastRandomFile=mCurrentFile;
                }

                //lastrandom file gets empty as soon as we reach the end of recursive scanning with movepath
                if (!mLastRandomFile.empty())
                {
                    DEB("filling random playlist");
                    
                    //insert new entries randomly
                    //fill the list but if it takes too long, continue later 
                    ptime started=microsec_clock::local_time();
                    while(mNextFiles.size()<mRandomLength)
                    {
                        mLastRandomFile=movePath(mCurrentPath, mLastRandomFile, mSortField, NEXT, RECURSE, CsortedDir::ALL, DONT_LOOP);

                        if (mLastRandomFile.empty())
                        {
                            DEB("reached end of subdir scan.");
                            break;
                        }

                        //insert at random position:                        
                        list<path>::iterator nextFileI;
                        nextFileI=mNextFiles.begin();
                        boost::random::uniform_int_distribution<> dist(0, mNextFiles.size());
                        int rndCount=dist(mRandomGenerator);
                        while(rndCount)
                        {
                            rndCount--;
                            nextFileI++;
                        }
                        mNextFiles.insert(nextFileI, mLastRandomFile);

                        //we've taken our time, so send an event to continue next time:
                        if ((microsec_clock::local_time()-started).total_milliseconds()>100)
                        {
                            DEB("scan-timeout, random size=" << mNextFiles.size() << " milliseconds taken=" << (microsec_clock::local_time()-started).total_milliseconds());
                            needs_more=true;
                            break;
                        }
                    }

                }
                else
                {
                    DEB("no more files left to fill random queue. random size");
                }
                DEB("random list size " <<  mNextFiles.size() );
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
                    if (p.empty())
                        break;

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
                    if (p.empty())
                        break;
                    mPrevPaths.push_back(p.string());
                }
            }

            return(needs_more);
        }


        //updates lists asyncroniously
        //call this with returned=false
        bool mUpdateListsAsyncFlying;

        void updateListsAsync(bool returned=false)
        {

            //this means we we're called by the actual event
            if (returned)
            {
                mUpdateListsAsyncFlying=false;
            }

            //always do at least one update when its called
            if (updateLists())
            {
                //more updates are needed, so send an updatelists event, if there's not already one flying. (we dont want the to queue up)
                if (!mUpdateListsAsyncFlying)
                {
                    mUpdateListsAsyncFlying=true;
                    Cmsg out;
                    out.event="pl_UpdateLists";
                    out.dst=mId;
                    out.send();
                }
            }
        }

        Citer()
        {
            mNextLen=5;
            mPrevLen=5;

            mRandomLength=0;
            mSortField="filename";
            mUpdateListsAsyncFlying=false;
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
                mCurrentFile=movePath(mCurrentPath, mCurrentPath, mSortField, NEXT, RECURSE, CsortedDir::ALL);
            }

            //fill the lists
            updateListsAsync();

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
            if (params.isSet("randomLength") && params["randomLength"]>=0 && params["randomLength"]<=10000)
            {
                mRandomLength=params["randomLength"];
            }

            if (params.isSet("sortField"))
                mSortField=params["sortField"].str();

            reloadPaths();
        }

		//next file
		void next()
		{
            if (mNextFiles.empty())
                return;

            mPrevFiles.push_front(mCurrentFile);
            mCurrentFile=mNextFiles.front();
            mNextFiles.pop_front();
            updateListsAsync();
		}

		//prev file
		void previous()
		{
            if (mPrevFiles.empty())
                return;

            mNextFiles.push_front(mCurrentFile);
            mCurrentFile=mPrevFiles.front();
            mPrevFiles.pop_front();
            updateListsAsync();
		}

		void nextPath()
		{
            if (mNextPaths.empty())
                return;
            mPrevPaths.push_front(mCurrentPath);
            mCurrentPath=mNextPaths.front();
                mNextPaths.pop_front();
			reloadFiles();

            //in random mode we want the first file to be random as well:
            if (mRandomLength)
                next();

		}

		void previousPath()
		{
            if (mPrevPaths.empty())
                return;
            mNextPaths.push_front(mCurrentPath);
            mCurrentPath=mPrevPaths.front();
            mPrevPaths.pop_front();
            reloadFiles();

            //in random mode we want the first file to be random as well:
            if (mRandomLength)
                next();
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
                //is the parent the currentpath?
                if (p.parent_path()==mCurrentPath)
                {
                    if (is_directory(p))
                    {
                        mCurrentPath=p;
                        reloadPaths();
                        return;
                    }
                    //the currentfile doesnt have a directory, so just use the first subdir we can find, if there is one
                    else
                    {
                        p=movePath(mCurrentPath, mCurrentPath, mSortField, NEXT, DONT_RECURSE, CsortedDir::DIR);
                        if (!p.empty())
                        {
                            mCurrentPath=p;
                            reloadPaths();
                        }
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
            if (
                    (!mCurrentPath.empty() && !isSubdir(mRootPath, mCurrentPath)) || 
                    (!mCurrentFile.empty() && !isSubdir(mRootPath, mCurrentFile))
                )
            {
                ERROR("escaped rootpath: " << mRootPath << " " << mCurrentPath << " " << mCurrentFile);
                throw(synapse::runtime_error("Program error: ended up outside rootpath. (dont use trailing slashes for rootpath)"));
            }

			Cmsg out;
			out.event="pl_Entry";
			out.dst=dst;
			out["rootPath"]=mRootPath.string();
            if (!mCurrentPath.empty())
                out["currentPath"]=mCurrentPath.string();

            if (!mCurrentFile.empty())
                out["currentFile"]=mCurrentFile.string();

            //to make life easier for user interfaces in a crossplatform way:
            out["parentPath"]=mCurrentPath.parent_path().string();


            int maxItems;

            maxItems=5;
            out["prevFiles"].list();
            BOOST_FOREACH(path prevFile, mPrevFiles)
            {
                out["prevFiles"].list().push_back(prevFile.string());
                maxItems--;
                if (maxItems==0)
                    break;
            }
    
            maxItems=5;
            out["nextFiles"].list();
            BOOST_FOREACH(path nextFile, mNextFiles)
            {
                out["nextFiles"].list().push_back(nextFile.string());
                maxItems--;
                if (maxItems==0)
                    break;
            } 

            maxItems=5;
            out["prevPaths"].list();
            BOOST_FOREACH(path prevPath, mPrevPaths)
            {
                out["prevPaths"].list().push_back(prevPath.string());
                maxItems--;
                if (maxItems==0)
                    break;
            }
    
            maxItems=5;
            out["nextPaths"].list();
            BOOST_FOREACH(path nextPath, mNextPaths)
            {
                out["nextPaths"].list().push_back(nextPath.string());
                maxItems--;
                if (maxItems==0)
                    break;
            }

            out["randomLength"]=mRandomLength;
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

    //used internally 
    SYNAPSE_REGISTER(pl_UpdateLists)
    {
        iterMan.get(dst).updateListsAsync(true);
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

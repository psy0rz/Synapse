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

#include <boost/regex.hpp> 

synapse::Cconfig config;

 
 /** Playlist namespace
 *
 */
namespace pl
{
	using namespace std;
	using namespace boost::filesystem;
	using namespace boost;
    using namespace boost::posix_time;


    //we dirive from directory_entry instead of path, since directory_entry caches extra data about the filetype (status())
	class Cdirectory_entry : public directory_entry
	{
		private:
		time_t mWriteDate;

		public:
//        Cdirectory_entry(const class path & p, file_status st, file_status symlink_st)
  //      :directory_entry(p, st, symlink_st)
          Cdirectory_entry(const directory_entry & e)
          :directory_entry(e)
        {
            mWriteDate=0;
        }

		//get/cache modification date
		int getDate()
		{
			if (!mWriteDate) 
				mWriteDate=last_write_time(this->path());

			return (mWriteDate);
		}

		//get sort filename string
		std::string getSortName()
		{
			return(path().filename().string());
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


    static map<path, list<Cdirectory_entry> > gDirCache;

    //reads one directory and apply appropriate filtering and sorting
	class CsortedDir: public list<Cdirectory_entry>
	{
		private:
		string mSortField;


        //read a directory without filtering and cache the results
        //FIXME: optimize and implement cleanup/refresh

        list<Cdirectory_entry> & readCached(path p)
        {
            if (gDirCache.find(p) == gDirCache.end())
            {
                DEB("Reading directory " << p.string());
                directory_iterator end_itr;
                for ( directory_iterator itr( p );
                    itr != end_itr;
                    ++itr )
                {
                    gDirCache[p].push_back(*itr);
                }
            }
            return gDirCache[p];
        }


		public:
        path mBasePath;
		enum Efiletype  { FILE, DIR, ALL };
        Efiletype mFiletype;
        regex mDirFilter;
        regex mFileFilter;
//		list<Cpath> mPaths;
//		list<Cpath>::iterator iterator;

		static bool compareFilename (Cdirectory_entry first, Cdirectory_entry second)
		{
			return (first.getSortName() < second.getSortName());
		}

		static bool compareDate (Cdirectory_entry first, Cdirectory_entry second)
		{
			return (first.getDate() > second.getDate());
		}

		void read(
            const path & basePath, 
            const string & sortField, 
            Efiletype filetype, 
            const regex  & dirFilter, 
            const regex & fileFilter)
		{
            //if nothing has changed, dont reread if its same 
            if (mBasePath==basePath && sortField==mSortField && filetype==mFiletype && fileFilter==mFileFilter && dirFilter == mDirFilter)
                return;

			mBasePath=basePath;
			mSortField=sortField;
            mFiletype=filetype;
            mFileFilter=fileFilter;
            mDirFilter=dirFilter;
            clear();

            list<Cdirectory_entry> & dir=readCached(basePath);

			for ( list<Cdirectory_entry>::iterator itr=dir.begin();
				itr != dir.end();
				++itr )
			{
                //dir
                if (is_directory(itr->status()))
                {
                    if (filetype==DIR || filetype==ALL)
                    {
                        if (mDirFilter.empty() || regex_search(itr->path().string(), mDirFilter , boost::match_any))
                        {
                            push_back(*itr);
                        }
                    }
                }
                //file
                else
                {
                    if (filetype==FILE || filetype==ALL)
                    {
//                        if (mFileFilter.empty() || regex_search(itr->path().filename().string(), mFileFilter , boost::match_any))
                        if (mFileFilter.empty() || regex_search(itr->path().string(), mFileFilter , boost::match_any))
                        {
                            push_back(*itr);
                        }
                    }
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
    };


    /*
    /a/b
    /a/b/c/d
    */

    /*** traverses directories/files

    -rootPath is the 'highest' path we can ever reach.
    -currentPath contains the currently 'selected' path. we start looking from here. we will also return the result in here
    -endPath is used internally to prevent infinite loops. set it to empty when call movePath for the first time.
    -filetype determines if we're looking for a file or directory or both.
    -direction tells if you want the next or previous file or directory.
    -recurse: its allow to enter or exit directories to find the next of previous file.
    -loop: when first or last path of the directory tree reached it roll around to the top or bottom. otherwise set currentPath to empty

    returns true if currentPath contains a valid result:
        when it fully loops and still found no result, currentPath will become empty and we will return true.

    returns false if we need to be called again:
        this happens in case of a timeout (currently 100mS)
        in this case we need to get called again with the same currentPath and endPath parameters.


    */


    enum Edirection { NEXT, PREVIOUS };
    enum Erecursion { RECURSE, DONT_RECURSE };
    enum Eloop      { LOOP, DONT_LOOP };

    static map<path, CsortedDir> gSortedDirCache;

    //recursive directory scanner, using non recursive functions. applies appropriate filtering
    class Cscanner
    {
        public: 


        //information that is determined by the user
        //these change all the time
        string mSortField;
        regex mDirFilter; //directory and file filter regexes
        regex mFileFilter;


        private:
        path mRootPath; //root path, we never go higher than this directory.

        //set at construction by the user, to determine the type of scanner 
        Edirection mDirection;
        Erecursion mRecursion;
        Eloop mLoop;
        CsortedDir::Efiletype mFileType;
 
        //state information which is needed while scanning
        path mStartSelectedPath; //used for loop detection
        path mStartListPath;
        path mCurrentSelectedPath; //path that is currently 'selected'.
        path mCurrentListPath; //path we use to list files and directorys
        bool mDone; //done scanning

        //CsortedDir  mSortedDir;


        public: 

        Cscanner(Edirection direction, Erecursion recursion, Eloop loop, CsortedDir::Efiletype fileType)
        {
            mDirection=direction;
            mRecursion=recursion;
            mLoop=loop;
            mFileType=fileType;
            mDirFilter=regex();
            mFileFilter=regex();            
            mDone=true;
        }

        //reset scanner status. use this after you change something
        void reset()
        {
            mDone=false;
            mStartSelectedPath.clear();
            mStartListPath.clear();

            if (mCurrentSelectedPath.empty())
                mCurrentListPath=mRootPath;
            else
            {
                //dont escape rootpath
                if (isSubdir(mRootPath, mCurrentSelectedPath.parent_path()))
                    mCurrentListPath=mCurrentSelectedPath.parent_path();
                else
                    mCurrentListPath=mRootPath;
            }
                
        }

        void setSelectedPath(path selectedPath)
        {
            mCurrentSelectedPath=selectedPath;
            reset();
        }

        void setRootPath(path rootPath)
        {
            mCurrentSelectedPath.clear();
            mRootPath=rootPath;
            reset();
        }


        path getRootPath()
        {
            return (mRootPath);
        }

        path getSelectedPath()
        {
            return (mCurrentSelectedPath);
        }

        //returns true when scan() needs te be called mode
        //this happens until:
        //-we've completely scanned all directories and files in mRootpath when mLoop is set to LOOP
        //-we've scanned until we reaced the bottom or top of mRootpath and mLoop is set to DONT_LOOP
        bool needsMore() 
        {
            return (!mDone);
        }

        //perform the actual scan, but not for more than the specified number of microseconds after starting.
        //if its finds something in time it returns true and getSelectedPath() will then return a path according to the search parameters
        //if it doesnt find anything, it will return false. when you call it the nexttime, it will continue scanning where it left off.
        bool scan(ptime startTime=microsec_clock::local_time(), int maxMillis=0)
        {
            //we're already done
            if (mDone)
                return (false);

            CsortedDir::iterator dirI;

            while(1)
            {
                if (maxMillis && (microsec_clock::local_time()-startTime).total_milliseconds()>maxMillis)
                {
                    //timeout, we will continue on the next call. 
                    return(false);
                }

                //get sorted directory listing
                CsortedDir & mSortedDir=gSortedDirCache[mCurrentListPath];
                mSortedDir.read(mCurrentListPath, mSortField, mFileType, mDirFilter, mFileFilter);

                //directory not empty
                if (!mSortedDir.empty())
                {
                    if (!mCurrentSelectedPath.empty())
                    {
                        //try to find the current path:
                        for (dirI=mSortedDir.begin(); dirI!=mSortedDir.end(); dirI++)
                        {
                            if (dirI->path()==mCurrentSelectedPath)
                                break;
                        }
                    }
                    else
                        dirI=mSortedDir.end();

                    //currentPath not found?
                    if (dirI==mSortedDir.end())
                    {
                        //start at the first or last entry depending on direction
                        if (mDirection==NEXT)
                            dirI=mSortedDir.begin();
                        else
                        {
                            dirI=mSortedDir.end();
                            dirI--;
                        }
                    }
                    else
                    {
                        //current path is found.

                        //make a step in the right direction
                        if (mDirection==NEXT)
                        {
                            dirI++;
                        }
                        //PREVIOUS:
                        else
                        {
                            if (dirI==mSortedDir.begin())
                                dirI=mSortedDir.end(); //end means: no result
                            else
                                dirI--;
                        }
                    }

                    //top or bottom was reached, or no result for some other reason
                    if (dirI==mSortedDir.end())
                    {
                        //can we one dir higher?
                        if (mRecursion==RECURSE && mCurrentListPath!=mRootPath)
                        {

                            // go one dir higher and continue the loop
                            mCurrentSelectedPath=mCurrentListPath;
                            mCurrentListPath=mCurrentListPath.parent_path();

                        }
                        else
                        {
                            //no, cant go higher.
                            //may we loop?
                            if (mLoop==LOOP)
                            {  
                                mCurrentSelectedPath.clear();
                            }
                            else
                            {
                                DEB("end reached, not looping, done");
                                mDone=true;
                                return(false);
                            }
                        }
                    }
                    //we found something
                    else
                    {
                        //should we recurse?
                        if (mRecursion==RECURSE && is_directory(dirI->status()))
                        {
                            //enter it
                            mCurrentSelectedPath.clear();
                            mCurrentListPath=(dirI->path());

                            //infinite loop detection
                            if (mStartListPath.empty())
                                mStartListPath=mCurrentListPath;
                            else
                            {
                                if (mStartListPath==mCurrentListPath)
                                {
                                    //we've looped
                                    DEB("looped all directories, we're done");
                                    mDone=true;
                                    return(false);
                                }
                            }

                            DEB("started "<< mStartListPath);
                            DEB("current "<< mCurrentListPath);


                        }
                        else
                        {
                            //we got an actual result for the user
                            mCurrentSelectedPath=dirI->path();

                            //only process all entries one time, prevent infinite looping
                            if (mStartSelectedPath.empty())
                                mStartSelectedPath=mCurrentSelectedPath;
                            else
                            {
                                if (mStartSelectedPath==mCurrentSelectedPath)
                                {
                                    //we've looped
                                    DEB("processed all items, we're done");
                                    mDone=true;
                                    return(false);
                                }
                            }

                            DEB("found, returning " << mCurrentSelectedPath);

                            return (true);
                        }
                    }
                }
                //directory empty
                else
                {
                    //dir is empty, our last chance is to go one dir higher, otherwise we will exit the loop:
                    if (mRecursion==RECURSE && mCurrentListPath!=mRootPath)
                    {
                        //go one dir higher and continue the loop
                        mCurrentSelectedPath=mCurrentListPath;

                        //infinite loop prevention, prevent leaving this directory for the second time. (first time is ok)
/*                        if (endPath.empty())
                            endPath=listPath;
                        else if (endPath==listPath)
                        {
                            DEB("came full circle without results, returing empty result");
                            currentPath.clear();
                            return(true);
                        }*/

                        mCurrentListPath=mCurrentListPath.parent_path();
                    }
                    else
                    {
                        DEB("empty directory and not recursing, done");
                        mDone=true;
                        return(false);
                    }
                }

            }
        }
    };

/*
    bool movePath(
        const path & rootPath, 
        path & currentPath, 
        path & endPath,
        string sortField, 
        Edirection direction, 
        Erecursion recursion, 
        CsortedDir::Efiletype filetype, 
        Eloop loop=LOOP, 
        const regex & dirFilter=regex(),
        const regex & fileFilter=regex()
    )
    {
        ptime started=microsec_clock::local_time();

        DEB("movePath rootPath=" << rootPath.string() <<
             " currentPath=" << currentPath.string() <<
             " endPath=" << endPath.string() <<
             " sortField=" << sortField <<
             " direction=" << direction <<
             " recursion=" << recursion << 
             " filetype=" << filetype << 
             " loop=" << loop);

        if (currentPath.empty())
        {
            DEB("currentpath empty, returning empty as well");
            return (true);
        }


        //determine the path we should get the initial listing of:
        path listPath;
        if (currentPath==rootPath)
            listPath=currentPath;
        else
            listPath=currentPath.parent_path();
        
        CsortedDir::iterator dirI;

        while(1)
        {
            //get sorted directory listing, but cache results per path for performance
            static map<path, CsortedDir>  sortedDirCache;
            CsortedDir & sortedDir=sortedDirCache[listPath];

            sortedDir.read(listPath, sortField, filetype, dirFilter, fileFilter);
            DEB("start of loop");
            DEB("list path :     "  << listPath);
            DEB("end path:     " << endPath);
            DEB("current path:   " << currentPath);

            //directory not empty
            if (!sortedDir.empty())
            {
                if (!currentPath.empty())
                {
                    //try to find the current path:
                    for (dirI=sortedDir.begin(); dirI!=sortedDir.end(); dirI++)
                    {
                        if (dirI->path()==currentPath)
                            break;
                    }
                }
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
                        //may we loop?
                        if (loop==LOOP)
                        {  
                            //yes
                            //currentPath.clear();
                            ;
                        }
                        else
                        {
                            DEB("end reached, not looping, so returning empty path");
                            currentPath.clear();
                            return(true);
                        }
                    }
                }
                //we found something
                else
                {
                    currentPath=(dirI->path());
                    //should we recurse?
                    if (recursion==RECURSE && is_directory(dirI->status()))
                    {
                        //enter it
                        listPath=(dirI->path());
                    }
                    else
                    {
                        //we found it
                        DEB("found, returning " << currentPath);
                        return (true);
                    }
                }
            }
            //directory empty
            else
            {
                //list is empty, our last chance is to go one dir higher, otherwise we will exit the loop:
                if (recursion==RECURSE && listPath!=rootPath)
                {
                    //go one dir higher and continue the loop
                    currentPath=listPath;

                    //infinite loop prevention, prevent leaving this directory for the second time. (first time is ok)
                    if (endPath.empty())
                        endPath=listPath;
                    else if (endPath==listPath)
                    {
                        DEB("came full circle without results, returing empty result");
                        currentPath.clear();
                        return(true);
                    }

                    listPath=listPath.parent_path();
                }
                else
                {
                    DEB("empty directory, returning empty result");
                    currentPath.clear();
                    return(true);
                }
            }

            if ((microsec_clock::local_time()-started).total_milliseconds()>100)
            {
                //timeout, return false. we will continue on the next call. (if we're called with the same currentPath)
                return(false);
            }
        }
    }
*/
    

    //play list 
	class Cpl
	{
		private:
		path mRootPath;
		path mCurrentPath;
		path mCurrentFile;

        list<path> mPrevFiles;
        Cscanner mPrevFilesScanner;

        list<path> mNextFiles;
        Cscanner mNextFilesScanner;

        list<path> mPrevPaths;
        Cscanner mPrevPathsScanner;

        list<path> mNextPaths;
        Cscanner mNextPathsScanner;

		int mId;

        unsigned int mNextLen;
        unsigned int mPrevLen;

        boost::random::mt19937 mRandomGenerator;
//        string mState["sortField"];
//        unsigned int mState["randomLength"]; //max length of random queue, the more, the better it is, but takes more time scanning 
        path mStartRandomFile; //path where we started scanning for random file. (to prevent loops and duplicate files)
        path mLastRandomFile; //last queued random path

        //filters
        regex mFileFilter; 
        regex mDirFilter; 

        //when was the last update sended?
        ptime mLastSend;


        public:

        //state info, used to store last selected file from every currentpath
        synapse::Cconfig mState;


        Cpl() : 
            mPrevFilesScanner(PREVIOUS, RECURSE, LOOP, CsortedDir::ALL),
            mNextFilesScanner(NEXT,     RECURSE, LOOP, CsortedDir::ALL),
            mPrevPathsScanner(PREVIOUS, DONT_RECURSE, DONT_LOOP, CsortedDir::DIR),
            mNextPathsScanner(NEXT,     DONT_RECURSE, DONT_LOOP, CsortedDir::DIR)
        {
            mNextLen=10;
            mPrevLen=10;

            mState["randomLength"]=0;
            mState["sortField"]="filename";
            mUpdateListsAsyncFlying=false;

            //paths always sorted by file, for now
            mNextPathsScanner.mSortField="filename";
            mPrevPathsScanner.mSortField="filename";

        }

	
        //make sure there are enough entries in the file and path lists
        //when it returns true, it should be called again. (used for scanning in small increments to improve response time)
        bool updateLists()
        {
            ptime started=microsec_clock::local_time();

            //next path list:
            while (mNextPaths.size()>mNextLen)
                mNextPaths.pop_back();

            while(mNextPaths.size()<mNextLen)
            {
                if (mNextPathsScanner.scan(started, 100))
                    mNextPaths.push_back(mNextPathsScanner.getSelectedPath());
                else if (mNextPathsScanner.needsMore())
                    return(true); //timeout
                else
                    break; //scanner is done
            }
             
            //prev path list:
            while (mPrevPaths.size()>mPrevLen)
                mPrevPaths.pop_back();

            while(mPrevPaths.size()<mPrevLen)
            {
                if (mPrevPathsScanner.scan(started, 100))
                    mPrevPaths.push_back(mPrevPathsScanner.getSelectedPath());   
                else if (mPrevPathsScanner.needsMore())
                    return(true);
                else
                    break;
            }


            //normal mode:
            if ((int)mState["randomLength"]==0)
            {
                //next file list:
                // while (mNextFiles.size()>mNextLen)
                //     mNextFiles.pop_back();

                while(mNextFiles.size()<mNextLen)
                {
                    if (mNextFilesScanner.scan(started, 100))
                    {
                        //if we dont have a currentFile yet, then use the first one we encounter.
                        if (mCurrentFile.empty())
                            mCurrentFile=mNextFilesScanner.getSelectedPath();
                        else
                            mNextFiles.push_back(mNextFilesScanner.getSelectedPath());
                    }
                    else
                    {
                        if (mNextFilesScanner.needsMore())
                            return(true); //timeout
                        else
                            break; //scanner is done
                    }
                }
                 
                //prev file list:
                // while (mPrevFiles.size()>mPrevLen)
                //     mPrevFiles.pop_back();

                while(mPrevFiles.size()<mPrevLen)
                {
                    if (mPrevFilesScanner.scan(started, 100))
                        mPrevFiles.push_back(mPrevFilesScanner.getSelectedPath());
                    else if (mPrevFilesScanner.needsMore())
                        return(true); //timeout
                    else 
                        break; //scanner is done
                }

            }
            //random mode
            else
            {
                //make sure prevlist is not too long
                while (mPrevFiles.size()>mState["randomLength"])
                    mPrevFiles.pop_back();


                //insert new entries randomly
                while(mNextFiles.size()<mState["randomLength"])
                {
                    if (mNextFilesScanner.scan(started, 100))
                    {
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
                        mNextFiles.insert(nextFileI, mNextFilesScanner.getSelectedPath());

                    }
                    else if (mNextFilesScanner.needsMore())
                        return(true); //timeout
                    else 
                        break; //scanner is done
                }
            }


            return(false);
        }


        //updates lists asyncroniously
        //call this with returned=false
        bool mUpdateListsAsyncFlying;

        void updateListsAsync(bool returned=false)
        {
            bool wantMore=false;

            //this means we we're called by the actual event
            if (returned)
            {
                mUpdateListsAsyncFlying=false;

                wantMore=updateLists();
            }
            else
            {
                wantMore=true;
            }
            //more updates are needed, so send an updatelists event, if there's not already one flying. (we dont want the to queue up)
            if (wantMore && !mUpdateListsAsyncFlying)
            {
                mUpdateListsAsyncFlying=true;
                Cmsg out;
                out.event="pl_UpdateLists";
                out.dst=mId;
                out.send();
            }

            
            if (
                !returned || //send the initial state instantly after the first call
                ((microsec_clock::local_time()-mLastSend).total_milliseconds()>200) || //send at least one update every second
                !wantMore //always send a final state when done
            )
            {
                mLastSend=microsec_clock::local_time();
                send(0);
            }
        };

        //gets reference to state object for specified path
        Cvar & getPathState(path p)
        {
            //we could hash the path as well, if performance/size gets an issue
            return(mState["paths"][p.string()]);
        }

        //name of the config file that stores the state
        string getStateFile()
        {
            //stringstream s;
            //s << mRootPath/".mp.state" << "/.mp.mState";
            return ((mRootPath/".mp.state").string());
        }

		//reload all file data. call this after you've changed currentPath or other settings
		void reloadFiles()
		{
			//clear lists
			mNextFiles.clear();
            mPrevFiles.clear();

            mNextFilesScanner.mSortField=mState["sortField"].str();
            mPrevFilesScanner.mSortField=mState["sortField"].str();

            mNextFilesScanner.setRootPath(mCurrentPath);
            mPrevFilesScanner.setRootPath(mCurrentPath);

            //currentfile is not a subdir of mCurrentPath?
            if (!isSubdir(mCurrentPath, mCurrentFile))
            {
                //did we store the previous file that was selected in this path?
                if (getPathState(mCurrentPath)["currentFile"]!="")
                {
                    setCurrentFile(getPathState(mCurrentPath)["currentFile"].str());
                }
                else
                    setCurrentFile(path()); //let the scanners figure it out
            }

            //its allowed to setSelectedPath to an empty path:
            mNextFilesScanner.setSelectedPath(mCurrentFile);
            mPrevFilesScanner.setSelectedPath(mCurrentFile);

            //fill the lists
            updateListsAsync();

		}

		//reload path and file data, call this after you exited or entered a path so that the previous/next paths need to be reloaded 
		void reloadPaths()
		{
			mNextPaths.clear();
            mPrevPaths.clear();

            //this is the usual case: there is some path selected, so we set the rootPath of the directory-scanners to the parent:
            if (isSubdir(mRootPath, mCurrentPath.parent_path()))
            {
                mNextPathsScanner.setRootPath(mCurrentPath.parent_path());
                mPrevPathsScanner.setRootPath(mCurrentPath.parent_path());
            }
            //user probably has selected the highest path, so we cant use the parent in this case. (since we never may escape mRootPath)
            else
            {
                mNextPathsScanner.setRootPath(mRootPath);
                mPrevPathsScanner.setRootPath(mRootPath);
            }

            mNextPathsScanner.setSelectedPath(mCurrentPath);
            mPrevPathsScanner.setSelectedPath(mCurrentPath);
			reloadFiles();
		}

        void setMode(Cvar params)
        {
            if (params.isSet("randomLength") && params["randomLength"]>=0 && params["randomLength"]<=10000)
            {
                mState["randomLength"]=params["randomLength"];
            }

            if (params.isSet("sortField"))
            {
                mState["sortField"]=params["sortField"].str();
            }

            reloadFiles();
        }

        //use this to change mCurrentFile 
        void setCurrentFile(path p)
        {
            if (!p.empty() && !isSubdir(mRootPath,p))
                throw(synapse::runtime_error("path outside rootpath"));

            mCurrentFile=p;
            getPathState(mCurrentPath)["currentFile"]=p.string();

        }

        //use this to change mCurrentPath
        void setCurrentPath(path p)
        {
            if (!p.empty() && !isSubdir(mRootPath,p))
                throw(synapse::runtime_error("path outside rootpath"));

            mCurrentPath=p;
            mState["currentPath"]=p.string();
            

        }

		//next file
		void next()
		{
            if (mNextFiles.empty())
                return;

            mPrevFiles.push_front(mCurrentFile);
            setCurrentFile(mNextFiles.front());
            mNextFiles.pop_front();

            //time to regenerate the random list?
            //note: we restart the scanner after its empty, so we never get the same file more than once in the random list. (because thats so annoying ;)
            if (mNextFiles.empty())
                mNextFilesScanner.reset();


            updateListsAsync(); //make sure the lists stay filled
		}

		//prev file
		void previous()
		{
            if (mPrevFiles.empty())
                return;

            mNextFiles.push_front(mCurrentFile);
            setCurrentFile(mPrevFiles.front());
            mPrevFiles.pop_front();
            updateListsAsync(); //make sure the lists stay filled
		}

        //goto start of the file list
        void gotoStart()
        {
            setCurrentFile(path());
            reloadFiles();
        }

		void nextPath()
		{
            if (mNextPaths.empty())
                return;

            mPrevPaths.push_front(mCurrentPath);
            mCurrentPath=mNextPaths.front();
            mNextPaths.pop_front();
            reloadFiles();
		}

		void previousPath()
		{
            if (mPrevPaths.empty())
                return;
            mNextPaths.push_front(mCurrentPath);
            mCurrentPath=mPrevPaths.front();
            mPrevPaths.pop_front();
            reloadFiles();
		}

		void exitPath()
		{
			if (isSubdir(mRootPath, mCurrentPath.parent_path()))
			{
				setCurrentPath(mCurrentPath.parent_path());
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
                        setCurrentPath(p);
                        reloadPaths();
                        return;
                    }
                    else
                    {
                        break;
                    }
                }
                p=p.parent_path();
            }

            //find the first valid directory in mCurrentPath and make that the new currentpath:
            mNextPathsScanner.setRootPath(mCurrentPath);
            if (mNextPathsScanner.scan())
            {
                //found something, set new path to it
                setCurrentPath(mNextPathsScanner.getSelectedPath());
            }
            reloadPaths(); //always reload, since we've abused nextPathScanner.
		}

        //sets regex for the file filter.
        //this is combined with the filter defined in config["filter"]
        void setFileFilter(string f)
        {
            //FIXME: now we just assume the default filter has a certain format
            string regexStr;
            if (f!="")
                regexStr=".*"+f+".*"+config["filter"].str();
            else
                regexStr=config["filter"].str();
            DEB("Setting file regex to: " << regexStr)
            mNextFilesScanner.mFileFilter.assign(regexStr, regex::icase);
            mPrevFilesScanner.mFileFilter.assign(regexStr, regex::icase);
            mState["fileFilter"]=f;
            reloadFiles();
        }

		void create(int id, string rootPath)
		{
			mId=id;
			mRootPath=rootPath;

            if (is_regular(getStateFile()))
            {
                mState.load(getStateFile());
            }
            
            if (isSubdir(mRootPath, mState["currentPath"].str()))
                setCurrentPath(mState["currentPath"].str());
            else
                setCurrentPath(mRootPath);

            setFileFilter(mState["fileFilter"]);

            reloadPaths();
			DEB("Created pl " << id << " for path " << rootPath);
			
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
            {
                out["currentPath"]=mCurrentPath.string();
                //to make life easier for user interfaces in a crossplatform way:
                out["parentPath"]=mCurrentPath.parent_path().string();
            }

            if (!mCurrentFile.empty())
                out["currentFile"]=mCurrentFile.string();


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
  
            out["randomLength"]=mState["randomLength"];
            out["sortField"]=mState["sortField"];
            out["fileFilter"]=mState["fileFilter"];

            //this means we're still scanning
            out["updateListsAsyncFlying"]=mUpdateListsAsyncFlying;

			out.send();
		}

		void destroy()
		{
            mState.changed();
            mState.save(getStateFile());

		}

	};

	class CplMan
	{
		private:
		typedef map<int,Cpl> CplMap;
		CplMap mPlMap;

		public:

		Cpl & get(int id)
		{
			if (mPlMap.find(id)==mPlMap.end())
				throw(synapse::runtime_error("Playlist not found"));
			return(mPlMap[id]);
		}

		void create(int id, string basePath)
		{
			if (mPlMap.find(id)!=mPlMap.end())
				throw(synapse::runtime_error("Playlist already exists"));

			mPlMap[id].create(id, basePath);
		}

		void destroy(int id)
		{
			get(id).destroy();
			mPlMap.erase(id);
		}


	};

	CplMan plMan;

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
            plMan.create(dst, msg["path"]);
        else
            plMan.create(dst, config["path"]);

        plMan.get(dst).send(0);
    }

    SYNAPSE_REGISTER(module_SessionEnd)
    {
        plMan.destroy(dst);
    }

	/** Create a new pl
		\param id Traverser id
		\path path Base path. pl can never 'escape' this directory.

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


	/** Delete specified pl (actually ends session, so you cant destroy default pl)
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
        plMan.get(dst).updateListsAsync(true);
    }

	/** Change selection/search criteria for files. Initalise a new pl
        \param s

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_SetMode)
	{
        plMan.get(dst).setMode(msg);

	}

    SYNAPSE_REGISTER(pl_SetFileFilter)
    {
        plMan.get(dst).setFileFilter(msg["fileFilter"]);

    }


	/** Get current directory and file
		\param id Traverser id

	\par Replies pl_Entry.
	*/
	SYNAPSE_REGISTER(pl_GetStatus)
	{
		plMan.get(dst).send(msg.src);
	}

	/** Select next directory entry in list
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_NextPath)
	{
		plMan.get(dst).nextPath();

	}


	/** Select previous entry directory in list
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_PreviousPath)
	{
		plMan.get(dst).previousPath();
	}

	/** Enters selected directory
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_EnterPath)
	{
		plMan.get(dst).enterPath();
	}

	/** Exits directory, selecting directory on higher up the hierarchy
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_ExitPath)
	{
		plMan.get(dst).exitPath();

	}

	/** Next song
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_Next)
	{
		plMan.get(dst).next();

	}

	/** Previous song
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_Previous)
	{
		plMan.get(dst).previous();
	}

    SYNAPSE_REGISTER(pl_GotoStart)
    {
        plMan.get(dst).gotoStart();
    }



	SYNAPSE_REGISTER(module_Shutdown)
	{
		shutdown=true;
	}

}

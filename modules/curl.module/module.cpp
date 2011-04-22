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


#include "synapse.h"
#include "cconfig.h"
#include "clog.h"
#include <map>
#include <string>
#include <deque>

#include "exception/cexception.h"

//include curl_ssl_lock to initalise correct thread locking for the ssl libraries
#define USE_OPENSSL
#include <curl/curl.h>
#include "curl_ssl_lock.hpp"

#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>


namespace synapse_curl
{

using namespace std;
using namespace boost;

//A curl instance
//TODO: make it so that new requests can get queued and the CURL handle will be reused. (more efficient and reuses cookies and stuff)
class Ccurl
{
	private:
	shared_ptr<mutex> mMutex;
	shared_ptr<condition_variable> mQueueChanged;


	bool mAbort;		//try to abort this transfer
	bool mPerforming; 	//we have assigned a performer
	deque<Cmsg> mQueue;	//queue with operations to perform
	CURL *mCurl;



	public:

	Ccurl()
	{
		mAbort=false;
		mPerforming=false;
		mMutex=shared_ptr<mutex>(new mutex);
		mQueueChanged=shared_ptr<condition_variable>(new condition_variable);

		mCurl=curl_easy_init();
		if (!mCurl)
			throw(synapse::runtime_error("Error creating new curl instance"));

	}

	//aborts all downloads and empties queue
	void abort()
	{
		lock_guard<mutex> lock(*mMutex);
		mAbort=true;
		mQueue.empty();
		mQueueChanged->notify_one();
	}

	//enqueues a download. returns true if the calling thread should call perform().
	bool enqueue(Cmsg & msg)
	{
		lock_guard<mutex> lock(*mMutex);
		mQueue.push_back(msg);
		//make sure messages can send back correctly:
		mQueue.end()->dst=mQueue.end()->src;
		mQueue.end()->src=0;

		mQueueChanged->notify_one();

		//no performing thread yet?
		if (!mPerforming)
		{
			mPerforming=true;
			return(true);
		}

		return(false);
	}

	//returns true if perform() should be called again
	//only call this when not already performing. when it returns false , curl is uninitalized so dont call perform or anything else again.
	bool shouldPerform()
	{
		lock_guard<mutex> lock(*mMutex);
		if (mQueue.empty())
		{
			curl_easy_cleanup(mCurl);
			return (false);
		}
		else
		{
			return(true);
		}
	}

	//only one thread may call perform at a time: this is indicated with enqueue() returning true
	//as long as shouldPerform() returns true you should recall this function, without any locking at all.
	//(the other public functions are threadsafe)
	void perform()
	{
		CURLcode err;

		{
			unique_lock<mutex> lock(*mMutex);

			//nothing to do?
			if (mQueue.empty())
			{
				//wait until someone throws something in the queue, or close this session
				if (mQueueChanged->timed_wait(lock,boost::date_time::duration_traits_duration))
					DEB("New data in the queue");
				else
					DEB("Idle, no date in the queue in time");
			}

			//still empty?
			if (mQueue.empty())
				return;

			//set curl options, as long as there are no errors
			(err=curl_easy_setopt(mCurl, CURLOPT_NOSIGNAL, 1))==0 &&
			(err=curl_easy_setopt(mCurl, CURLOPT_VERBOSE, 1))==0 &&
			(err=curl_easy_setopt(mCurl, CURLOPT_URL, (*mQueue.begin())["url"].str().c_str() ))==0;

			//indicate start
			(*mQueue.begin()).event="curl_Start";
			(*mQueue.begin()).send();
		}

		//perform the operation (unlocked)
		if (err==0)
			err=curl_easy_perform(mCurl);

		{
			unique_lock<mutex> lock(*mMutex);

			if (err==0)
			{
				//ok, indicate done
				(*mQueue.begin()).event="curl_Ok";
				(*mQueue.begin()).send();
			}
			else
			{
				//error, indicate error
				(*mQueue.begin()).event="curl_Error";
				(*mQueue.begin())["error"]=curl_easy_strerror(err);
				(*mQueue.begin()).send();
			}

			//remove from queue
			mQueue.pop_front();
		}
	}
};


mutex curlMapMutex;
typedef pair<int, string> CcurlKey;
typedef map<CcurlKey, Ccurl> CcurlMap;
CcurlMap curlMap;
int defaultSession;



SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	defaultSession=dst;

	//load config file
	synapse::Cconfig config;
	config.load("etc/synapse/curl.conf");

	init_locks();
	if (curl_global_init(CURL_GLOBAL_ALL)!=0)
		throw(synapse::runtime_error("Error while initializing curl"));

	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=config["maxProcesses"];
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=config["maxProcesses"];
	out.send();

	out.clear();
	out.event="core_Ready";
	out.send();

}

SYNAPSE_REGISTER(module_Shutdown)
{
	curl_global_cleanup();
	kill_locks();
}


Ccurl & getCurl(int src, string & id)
{
	if (curlMap.find(CcurlKey(src,id))==curlMap.end())
		throw(synapse::runtime_error("Download id not found"));

	return(curlMap.find(CcurlKey(src,id))->second);
}

/** Get specified url
Downloads the specified url.

	\param id uniq identifier to reconginise the download. multiple downloads with the same ID from the same source session will be queued.
	\param url The url to download

\par Replys:
	In every reply all the parameters used in the curl_Get are returned.

	curl_Start	Indicates start of download
	curl_Data	Sended for received data. \param data will contain the data.
	curl_Ok		Indicates downloading is ready
	curl_Error  Indicates downloading is aborted.
 */
SYNAPSE_REGISTER(curl_Get)
{
	CcurlKey key(msg.src, msg["id"]);
	CcurlMap::iterator curlI=curlMap.end();
	bool perform=true;

	{
		lock_guard<mutex> lock(curlMapMutex);

		//returns true if we should call perform
		perform=curlMap[key].enqueue(msg);

		curlI=curlMap.find(key);
	}

	//loop while we should perform downloads
	while (perform)
	{
		//call it unlocked! (since this is the thing that takes a long time)
		curlI->second.perform();

		{
			lock_guard<mutex> lock(curlMapMutex);

			//we need this construction to prevent the racecondition when something is enqueued just after perform has returned
			if (!curlI->second.shouldPerform())
			{
				//delete it
				curlMap.erase(curlI);
				perform=false;
			}
		}
	}
}


SYNAPSE_REGISTER(curl_Abort)
{
	lock_guard<mutex> lock(curlMapMutex);
	getCurl(msg.src,msg["id"]).abort();
}



//end namespace
}
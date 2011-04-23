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

synapse::Cconfig config;

//A curl instance
//TODO: make it so that new requests can get queued and the CURL handle will be reused. (more efficient and reuses cookies and stuff)
class Ccurl
{
	private:
	shared_ptr<mutex> mMutex;
	shared_ptr<condition_variable> mQueueChanged;


	bool mAbort;		//try to abort this transfer
	bool mPerforming; 	//we have assigned a performer
	typedef deque<Cmsg> Cqueue;

	Cqueue mQueue;	//queue with operations to perform
	CURL *mCurl;



	public:
	Cqueue::iterator mMsg; //current message being handled

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
		Cqueue::iterator lastMsg=(--mQueue.end());
		lastMsg->dst=lastMsg->src;
		lastMsg->src=0;

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
		unique_lock<mutex> lock(*mMutex);

		//nothing to do, wait a while for new messages
		//curl keeps connections open and remembers cookies and stuff.
		if (mQueue.empty())
		{
			//wait until someone throws something in the queue
			DEB("Queue empty, waiting...");
			if (mQueueChanged->timed_wait(lock,posix_time::seconds(10)))
				DEB("Queue filled again, continueing")
			else
				DEB("Queue wait timeout");
		}

		//still no data after waiting, delete curl instance and make sure we dont get called again
		if (mQueue.empty())
		{
			DEB("Queue still empty, cleaning up curl instance")
			curl_easy_reset(mCurl); //make sure the callbacks aren't called anymore!
			curl_easy_cleanup(mCurl);
			return (false);
		}
		else
		{
			return(true);
		}
	}

	static int curl_debug_callback (CURL * curl, curl_infotype type, char * data, size_t length, Ccurl *curlObj)
	{
		if (type==CURLINFO_TEXT)
		{
			string text;
			text.resize(length);
			memcpy((void *)text.c_str(),data,length);
			INFO("curl (" <<
					curlObj->mMsg->dst << " "  <<
					(*curlObj->mMsg)["id"].str() << ") " << text);
		}

		return 0;
	}

	//only one thread may call perform at a time: this is indicated with enqueue() returning true
	//as long as shouldPerform() returns true you should recall this function, without any locking at all.
	//(the other public functions are threadsafe)
	void perform()
	{
		CURLcode err;

		{
			lock_guard<mutex> lock(*mMutex);

			//empty?
			if (mQueue.empty())
				return;

			//activate message
			mMsg=mQueue.begin();

			//set curl options, as long as there are no errors
			curl_easy_reset(mCurl);
			(err=curl_easy_setopt(mCurl, CURLOPT_NOSIGNAL, 1))==0 &&
			(err=curl_easy_setopt(mCurl, CURLOPT_DEBUGFUNCTION, curl_debug_callback))==0 &&
			(err=curl_easy_setopt(mCurl, CURLOPT_DEBUGDATA, this))==0 &&
			//(err=curl_easy_setopt(mCurl, CURLOPT_NOSIGNAL, 1))==0 &&
			(err=curl_easy_setopt(mCurl, CURLOPT_VERBOSE, (int)config["verbose"]))==0 &&
			(err=curl_easy_setopt(mCurl, CURLOPT_URL, (*mMsg)["url"].str().c_str() ))==0;

			//indicate start
			mMsg->event="curl_Start";
			mMsg->send();
		}

		//perform the operation (unlocked)
		if (err==0)
			err=curl_easy_perform(mCurl);

		{
			unique_lock<mutex> lock(*mMutex);

			if (err==0)
			{
				//ok, indicate done
				mMsg->event="curl_Ok";
				mMsg->send();
			}
			else
			{
				//error, indicate error
				mMsg->event="curl_Error";
				(*mMsg)["error"]=curl_easy_strerror(err);
				mMsg->send();
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

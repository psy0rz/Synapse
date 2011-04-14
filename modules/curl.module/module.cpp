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
	bool mAbort;		//try to abort this transfer

	//throws error when a curl error is passed
	void curlThrow(CURLcode err)
	{
		if (err!=0)
		{
			throw(synapse::runtime_error(curl_easy_strerror(err)));
		}
	}

	public:

	Ccurl()
	{
		mAbort=false;
		mMutex=shared_ptr<mutex>(new mutex);
	}

	void abort()
	{
		lock_guard<mutex> lock(*mMutex);
		mAbort=true;
	}

	//only one thread may call perform at a time.
	//(the other public functions are threadsafe)
	void perform(Cmsg & msg)
	{
		CURL *pCurl;
		pCurl=curl_easy_init();
		if (!pCurl)
			throw(synapse::runtime_error("Error creating new curl instance"));

		//now we must call curl_easy_cleanup in case of exception:
		try
		{
			curlThrow(curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1));
			curlThrow(curl_easy_setopt(pCurl, CURLOPT_URL, msg["url"].str().c_str()));
			curlThrow(curl_easy_perform(pCurl));
		}
		catch(...)
		{
			curl_easy_cleanup(pCurl);
			throw;
		}

		curl_easy_cleanup(pCurl);

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

	init_locks();
	if (curl_global_init(CURL_GLOBAL_ALL)!=0)
		throw(synapse::runtime_error("Error while initializing curl"));


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
 * For now you only can have one download per id, and not queue downloads. (this might change but is more complex to implement.)
 *
 */
SYNAPSE_REGISTER(curl_Get)
{
	CcurlKey key(msg.src, msg["id"]);
	CcurlMap::iterator curlI;

	{
		lock_guard<mutex> lock(curlMapMutex);

		//already exists?
		if (curlMap.find(key)!=curlMap.end())
		{
			throw(synapse::runtime_error("Already downloading"));
		}

		//create it and get iterator
		curlMap[key];
		curlI=curlMap.find(key);
	}

	//perform download (UNLOCKED)
	curlI->second.perform(msg);

	{
		lock_guard<mutex> lock(curlMapMutex);
		//delete it
		curlMap.erase(curlI);
	}
}

SYNAPSE_REGISTER(curl_Abort)
{
	lock_guard<mutex> lock(curlMapMutex);
	getCurl(msg.src,msg["id"]).abort();
}



//end namespace
}

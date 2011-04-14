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

#include <curl/curl.h>


namespace synapse_curl
{

using namespace std;


//A curl instance
class Ccurl
{
	private:
	int mId;

	//pointers to vlc objects and event managers

	public:

};

typedef map<int, Ccurl> CcurlMap;
CcurlMap curls;
int defaultSession;



SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	defaultSession=dst;

	  curl_global_init(CURL_GLOBAL_ALL);

	out.clear();
	out.event="core_Ready";
	out.send();

}

SYNAPSE_REGISTER(module_Shutdown)
{
}


SYNAPSE_REGISTER(curl_Get)
{
//	players[msg.dst].stop();
}




//end namespace
}

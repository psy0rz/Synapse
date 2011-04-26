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
The twitter module.

This is a simple module that follows someones twitter feed.

TODO: add oauth or oauth2 support.

*/


#include "synapse.h"
#include "cconfig.h"
#include <time.h>


namespace twitter
{
using namespace std;


synapse::Cconfig config;
string userId;
enum eState {
		GET_USERID,
		GET_HISTORY,
		STREAM
};
eState state;
string queue;

//send resquest based on current state
void callTwitter()
{
	queue="";

	Cmsg out;
	out.event="curl_Get";
	out["id"]="twitter";

	if (state==GET_USERID)
	{
		out["url"]="http://api.twitter.com/1/users/show.json?screen_name="+config["follow"].str();
	}
	else if (state==GET_HISTORY)
	{
		out["url"]="http://api.twitter.com/1/statuses/user_timeline.json?trim_user=true&user_id="+userId;
	}
	else if (state==STREAM)
	{
//		out["httpauth"]="basic";
		out["username"]=config["username"];
		out["password"]=config["password"];
		out["url"]="http://stream.twitter.com/1/statuses/filter.json?follow="+userId;
//		out["url"]="http://stream.twitter.com/1/statuses/filter.json?track=twitter";

		//reset error status
		Cmsg err;
		err.event="twitter_Ok";
		out.send();


	}
	out.send();
}


SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	config.load("etc/synapse/twitter.conf");

	out.event="core_LoadModule";
	out["path"]="modules/curl.module/libcurl.so";
	out.send();




}

SYNAPSE_REGISTER(curl_Ready)
{
	Cmsg out;
	//tell the rest of the world we are ready for duty
	out.event="core_Ready";
	out.send();

	Cmsg err;
	err.event="twitter_Error";
	err["error"]="Connecting...";
	out.send();

	state=GET_USERID;
	callTwitter();
}


SYNAPSE_REGISTER(curl_Ok)
{
	try
	{
		//determine how to interpret the data:
		if (state==GET_USERID)
		{
			Cvar data;
			data.fromJson(queue);
			if (data.isSet("id_str"))
			{
				userId=data["id_str"].str();
				state=GET_HISTORY;
			}
		}
		else if (state==GET_HISTORY)
		{
			Cvar data;
			data.fromJson(queue);
			//traverse all the history items
			for (CvarList::iterator I=data.list().begin(); I!=data.list().end(); I++)
			{
				Cmsg out;
				out.event="twitter_Status";
				out.map()=I->map();
				out.send();
			}
			state=STREAM;
		}
		else if (state==STREAM)
		{
			//if we get disconnected, we need to reget history in case we missed something
			state=GET_HISTORY;
		}
	}
	catch(...)
	{
		sleep(5);
		callTwitter();
		throw;
	}
	callTwitter();

}

SYNAPSE_REGISTER(curl_Error)
{
	Cmsg out;
	out.event="twitter_Error";
	out["error"]=msg["error"];
	out.send();

	if (state==STREAM)
	{
		//if we get disconnected, we need to reget history in case we missed something
		state=GET_HISTORY;
	}
	sleep(5);
	callTwitter();
}

SYNAPSE_REGISTER(curl_Data)
{
	queue+=msg["data"].str();
	if(state==STREAM)
	{
		while (queue.find("\n")!=string::npos)
		{
			//get the jsonStr, delimited by \n:
			string jsonStr=queue.substr(0,queue.find("\n")+1);
			queue.erase(0,jsonStr.length());

			if (jsonStr.length()>2)
			{
				//convert to data
				Cvar data;
				data.fromJson(jsonStr);

				//send out message
				Cmsg out;
				out.event="twitter_Status";
				out.map()=data;
				out.send();
			}
		}
	}
}







}
;

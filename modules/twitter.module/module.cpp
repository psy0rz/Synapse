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
string userIds;
enum eState {
		GET_USERS,
		STREAM
};
eState state;
string queue;

//send resquest based on current state
SYNAPSE_REGISTER(twitter_Reconnect)
{
	queue="";

	Cmsg out;

	//abort old stuff
	out["id"]="twitter";
	out.event="curl_Abort";
	out.send();

	//add oauth parameters:
	out["oauth"]["consumer_key"]=config["oauth_consumer_key"];
	out["oauth"]["consumer_key_secret"]=config["oauth_consumer_key_secret"];
	out["oauth"]["token"]=config["oauth_token"];
	out["oauth"]["token_secret"]=config["oauth_token_secret"];

	if (state==GET_USERS)
	{
		out["url"]="http://api.twitter.com/1/users/lookup.json?screen_name="+config["follow"].str();
	}
	else if (state==STREAM)
	{
		out["url"]="http://stream.twitter.com/1/statuses/filter.json?follow="+userIds;

		//reset error status
		Cmsg err;
		err.event="twitter_Ok";
		err.send();
	}

	out.event="curl_Get";
	out.send();
}


SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	config.load("etc/synapse/twitter.conf");

	out.event="core_LoadModule";
	out["name"]="curl";
	out.send();
}

SYNAPSE_REGISTER(curl_Ready)
{
	Cmsg out;
	out.event="core_LoadModule";
	out["name"]="http_json";
	out.send();

}

SYNAPSE_REGISTER(http_json_Ready)
{
	Cmsg out;
	out.event="core_LoadModule";
	out["name"]="timer";
	out.send();
}

SYNAPSE_REGISTER(timer_Ready)
{

	Cmsg out;
	//tell the rest of the world we are ready for duty
	out.event="core_Ready";
	out.send();

	Cmsg err;
	err.event="twitter_Error";
	err["error"]="Connecting...";
	out.send();

	out.clear();
	state=GET_USERS;
	out.dst=msg.dst;
	out.event="twitter_Reconnect";
	out.send();

}


SYNAPSE_REGISTER(twitter_GetConfig)
{
	Cmsg out;
	out.dst=msg.src;
	out.event="twitter_Config";
	out.map()=config.map();
	out.send();
}

SYNAPSE_REGISTER(twitter_SetConfig)
{
	config.map()=msg.map();
	config.changed();
	config.save();


}


SYNAPSE_REGISTER(curl_Ok)
{
	try
	{
		//determine how to interpret the data:
		if (state==GET_USERS)
		{
			Cvar data;
			data.fromJson(queue);
			if (data.isSet("id_str"))
			{
				//userId=data["id_str"].str();
				state=STREAM;
			}
		}
//		else if (state==GET_HISTORY)
//		{
//			Cvar data;
//			data.fromJson(queue);
//			//traverse all the history items
//			for (CvarList::iterator I=data.list().begin(); I!=data.list().end(); I++)
//			{
//				Cmsg out;
//				out.event="twitter_Status";
//				out.map()=I->map();
//				out.send();
//			}
//			state=STREAM;
//		}
		else if (state==STREAM)
		{
			//if we get disconnected, we need to reget history in case we missed something
			state=GET_USERS;
		}
	}
	catch(...)
	{

		sleep(5);
		//callTwitter();
		throw;
	}
//	callTwitter();

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
		state=GET_USERS;
	}
	sleep(30);
//	callTwitter();
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

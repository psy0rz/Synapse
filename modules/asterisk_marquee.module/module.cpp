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
#include <string>
#include <map>
#include <set>

using namespace std;


SYNAPSE_REGISTER(module_Init)
{
	
	Cmsg out;
	
	out.clear();
	out.event="core_LoadModule";
  	out["name"]="marquee_m500";
  	out.send();
}


SYNAPSE_REGISTER(marquee_m500_Ready)
{
	Cmsg out;
	out.clear();
	out.event="core_LoadModule";
  	out["name"]="asterisk";
  	out.send();
}

SYNAPSE_REGISTER(asterisk_Ready)
{

	Cmsg out;

	out.clear();
	out.event="marquee_Set";
	out["text"]="ready";
	out.send();

	out.clear();
	out.event="asterisk_authReq";
	out.send();


	///tell the rest of the world we are ready for duty
	out.clear();
	out.event="core_Ready";
	out.send();

}

typedef map<string,Cvar> CchannelMap;
CchannelMap channels;


void updateMarquee()
{
	//make a unique list of the caller ids of ringing phones and a string with active calls.
	set<string> uniqCallerIds;
	string activeCalls;
	for (CchannelMap::iterator I=channels.begin(); I!=channels.end(); I++)
	{
		if (I->second["state"].str()=="ringing")
		{
			string callerId;
			//already linked with a channel?
			if (I->second["linkCallerId"].str() != "")
			{
				callerId=I->second["linkCallerId"].str();
			}
			//we dont know whos connected yet, try using the callerId
			else
			{
				callerId=I->second["callerId"].str();
			}

			//only show external calls. e.g. longer then 4 digits.
			if (callerId.length()>4)
				uniqCallerIds.insert(callerId);
		}
		else
		{
			string callerId;
			//we know who's connected:
			if (I->second["linkCallerId"].str() != "")
			{
				callerId=I->second["linkCallerId"].str();
			}
			//we dont know whos connected yet
			else
			{
				//show who's being dialed, or show caller id if its an incoming call:
				if (I->second["state"].str()=="out")
					callerId=I->second["firstExtension"].str();
				else
					callerId=I->second["callerId"].str();
			}


			if (I->second["state"].str()=="in")
				activeCalls=activeCalls + I->second["deviceCallerId"].str() + "<" + callerId + " ";
			else if (I->second["state"].str()=="out")
				activeCalls=activeCalls + I->second["deviceCallerId"].str() + ">" + callerId + " ";
		}
	}


	//are there incoming ringing calls?
	if (!uniqCallerIds.empty())
	{
		//transform the uniq list into a string
		string text;
		for (set<string>::iterator I=uniqCallerIds.begin(); I!=uniqCallerIds.end(); I++)
		{
			if (text=="")
				text=*I;
			else
				text=text+", "+*I;
		}
		Cmsg out;
		out.event="marquee_Set";
		out["text"]="%S1%C0"+text;
		out.send();
	}
	//nothing ringing, show active calls
	else
	{
		Cmsg out;
		out.event="marquee_Set";
		if (activeCalls=="")
		{
				out["text"]="%C7no calls";
		}
		else
		{
			out["text"]="%C3"+activeCalls;
		}
		out.send();
	}
}


SYNAPSE_REGISTER(asterisk_reset)
{
	channels.empty();
	updateMarquee();
}

SYNAPSE_REGISTER(asterisk_authCall)
{
	Cmsg out;
	out.event="marquee_Set";
	out["text"]="%S1Call "+msg["number"].str() + " to login...";
	out.send();
}

SYNAPSE_REGISTER(asterisk_authOk)
{
	Cmsg out;
	out.event="marquee_Set";
	out["text"]="Login ok";
	out.send();

	out.event="asterisk_refresh";
	out.send();
}

SYNAPSE_REGISTER(asterisk_updateChannel)
{
	channels[msg["id"]]=msg;
	updateMarquee();
}

SYNAPSE_REGISTER(asterisk_delChannel)
{
	channels.erase(msg["id"]);
	updateMarquee();
}


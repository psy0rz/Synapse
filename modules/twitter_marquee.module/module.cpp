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

using namespace std;

string statusStr;
string errorStr;
bool ok;

void setMarquee()
{
	Cmsg out;
	out.event="marquee_Set";
	out["classes"].list().push_back(string("twitter"));
	if (!ok)
		out["text"]=errorStr;
	else
		out["text"]=statusStr;

	out.send();

}

SYNAPSE_REGISTER(module_Init)
{
	ok=false;
	
	Cmsg out;
	out.event="core_LoadModule";
  	out["name"]="marquee_m500";
  	out.send();
}

SYNAPSE_REGISTER(marquee_m500_Ready)
{
	Cmsg out;
	out.event="core_LoadModule";
  	out["name"]="twitter";
  	out.send();

}

SYNAPSE_REGISTER(twitter_Ready)
{
	Cmsg out;
	out.event="core_Ready";
  	out.send();
}



SYNAPSE_REGISTER(twitter_Ok)
{
	ok=true;
	errorStr="";
	setMarquee();
}

SYNAPSE_REGISTER(twitter_Error)
{
	ok=false;
	errorStr=msg["error"].str();
	setMarquee();
}

SYNAPSE_REGISTER(twitter_Status)
{
	statusStr="%C2TWITTER  %C6"+msg["user"]["name"].str()+":%C0"+msg["text"].str();
	setMarquee();
}



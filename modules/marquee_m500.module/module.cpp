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

#include "cconfig.h"


using namespace std;
using namespace boost;

Cvar status;
bool changed;

synapse::Cconfig config;

SYNAPSE_REGISTER(module_Init)
{

	changed=true;
	
	Cmsg out;

	config.load("etc/synapse/marquee.conf");

	out.clear();
	out.event="core_LoadModule";
	out["name"]="timer";
	out.send();
}

SYNAPSE_REGISTER(timer_Ready)
{
	Cmsg out;
	out.clear();
	out.event="timer_Set";
	out["seconds"]=1;
	out["repeat"]=-1;
	out["dst"]=dst;
	out["event"]="marquee_SendChanges";
	out.send();

	///tell the rest of the world we are ready for duty
	out.clear();
	out.event="core_Ready";
	out.send();
}


SYNAPSE_REGISTER(marquee_Set)
{
	//filter classes?
	if (!config["classes"]["*"])
	{
		bool match=false;
		//do the specified classes match one of ours?
		for (Cvar::iterator I=msg["classes"].begin(); I!=msg["classes"].end(); I++)
		{
			if (I->second)
			{
				if (config["classes"].isSet(I->first) && config["classes"][I->first])
				{
					match=true;
					break;
				}
			}
		}
		if (!match)
		{
			DEB("Ignoring marquee message, no matching classes");
			return;
		}
	}

	if (status["text"].str()!=msg["text"].str())
	{
		status=msg;
		changed=true;
	}

}

SYNAPSE_REGISTER(marquee_Get)
{
	Cmsg out;
	out.dst=msg.src;
	out.event="marquee_Set";
	out.map()=status;
	out.send();
}

SYNAPSE_REGISTER(marquee_SendChanges)
{
	if (changed)
	{
		//HACK, implement in C++:
		if (!fork())
		{
			execlp("modules/marquee_m500.module/writemarquee", "writemarquee", status["text"].str().c_str(), NULL);
		}

		changed=false;
	}
}



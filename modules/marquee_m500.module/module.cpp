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

using namespace std;

string text;
bool changed;


SYNAPSE_REGISTER(module_Init)
{
	text="";
	changed=true;
	
	Cmsg out;

	out.clear();
	out.event="core_LoadModule";
	out["path"]="modules/timer.module/libtimer.so";
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
	if (text!=msg["text"].str())
	{
		text=msg["text"].str();
		changed=true;
	}

}

SYNAPSE_REGISTER(marquee_Get)
{
	Cmsg out;
	out.dst=msg.src;
	out.event="marquee_Set";
	out["text"]=text;
	out.send();
}

SYNAPSE_REGISTER(marquee_SendChanges)
{
	if (changed)
	{
		//HACK, implement in C++:
		string cmd;
		cmd="./writemarquee '"+text+"'";
		if (system(cmd.c_str())) ;

		changed=false;
	}
}



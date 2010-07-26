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
		cmd="/home/psy/opensyn3/trunk/projects/movingsign-m500N-7x80rg2/writemarquee '"+text+"'";
		if (system(cmd.c_str())) ;

		changed=false;
	}
}



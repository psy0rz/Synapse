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
  	out["path"]="modules/asterisk.module/libasterisk.so";
  	out.send();
}

//TODO: load libmarquee in between

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


map<string,string> callerIds;


SYNAPSE_REGISTER(asterisk_reset)
{
	callerIds.empty();
}

SYNAPSE_REGISTER(asterisk_authCall)
{
	Cmsg out;
	out.event="marquee_Set";
	out["text"]="Call "+msg["number"].str() + " to login...";
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

void updateMarquee()
{
	//make a unique list of caller Ids
	set<string> uniqCallerIds;
	for (map<string,string>::iterator I=callerIds.begin(); I!=callerIds.end(); I++)
	{
		uniqCallerIds.insert(I->second);
	}

	//now make a nice string for the marquee
	string text;
	for (set<string>::iterator I=uniqCallerIds.begin(); I!=uniqCallerIds.end(); I++)
	{
		if (text=="")
			text=*I;
		else
			text=text+", "+*I;
	}

	//determine the string to send to the marquee
	Cmsg out;
	out.event="marquee_Set";
	if (text!="")
		out["text"]="%C0"+text;
	else if (!callerIds.empty())
	{
		stringstream s;
		if (callerIds.size()==1)
			s << "%C31 call";
		else
			s << "%C3" << callerIds.size() << " calls";
		out["text"]=s.str();
	}
	else
		out["text"]="%C7no calls";
	out.send();

}

SYNAPSE_REGISTER(asterisk_updateChannel)
{
	string callerId;
	if (msg["state"].str()=="ringing")
	{
		//already linked with a channel?
		if (msg["linkCallerId"].str() != "")
		{
			callerId=msg["linkCallerId"].str();
		}
		//we dont know whos connected yet, try using the callerId
		else
		{
			callerId=msg["callerId"].str();
		}
	}

	//update the list
	callerIds[msg["id"]]=callerId;

	updateMarquee();
}

SYNAPSE_REGISTER(asterisk_delChannel)
{
	callerIds.erase(msg["id"]);
	updateMarquee();
}

SYNAPSE_REGISTER(net_Ready)
{

}

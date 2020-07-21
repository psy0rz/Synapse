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
#include <boost/preprocessor/cat.hpp>
#include <map>
#include <string>

using namespace std;
using namespace boost;

boost::mutex threadMutex;


SYNAPSE_REGISTER(module_Init)
{

	INFO("Regression test for: libnet loopback.");
	Cmsg out;

	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=10;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=10;
	out.send();

	out.clear();
	out.event="core_LoadModule";
	out.dst=1;
	out["path"]="modules/net.module/libnet.so";
	out.send();

	out.clear();
	out.event="core_ChangeLogging";
	out["logSends"]=0;
	out["logReceives"]=0;
	out.send();

}

#define NET_SESSIONS 30
int sessions=0;

SYNAPSE_REGISTER(net_Ready)
{
	Cmsg out;

	//start server on port 12345
	out.clear();
	out.event="net_Listen";
	out["port"]=12345;
	out.send();
	
	//wait for the message to get processed (also easier to debug)
	sleep(1);

	//accept connections
	for (int a=0; a<NET_SESSIONS; a++)
	{
		out.clear();
		out.event="core_NewSession";
		out["pars"]["mode"]="accept";
		out["description"]="test acceptor";
		out.send();
	};

	//wait for the message to get processed (also easier to debug)
	sleep(1);

	//create connections
	for (int a=0; a<NET_SESSIONS; a++)
	{
		out.clear();
		out.event="core_NewSession";
		out["pars"]["mode"]="connect";
		out["description"]="test connector";
		out.send();
	};

}

SYNAPSE_REGISTER(module_SessionStart)
{
	if (msg["pars"]["mode"].str()=="accept")
	{
		sessions++;
		Cmsg out;
		out.clear();
		out.event="net_Accept";
		out.src=msg.dst;
		out["port"]=12345;
		out.send();
	}
	else if (msg["pars"]["mode"].str()=="connect")
	{
		sessions++;
		Cmsg out;
		out.clear();
		out.event="net_Connect";
		out.src=msg.dst;
		out["port"]=12345;
		out["host"]="localhost";
		out.send();
	}
}

//keep track of the data-ordering
int lastId[10000];
bool failed=false;
int sessioneof=0;

SYNAPSE_REGISTER(net_Connected)
{
	lastId[msg.dst]=0;

	INFO("CONNECTED " << msg.dst << " sending data..");

	Cmsg out;
	out.src=msg.dst;
	out.dst=msg.src;
	out.event="net_Write";

	//send lines of text with an identifier, so we can check data order and integrety
	for (int a=1; a<=1000; a++)
	{
		out["data"]=a;
		out.send();
	}
	out["data"]="eof";
	out.send();

	INFO("SENDING FOR " << msg.dst << " completed..");
}


SYNAPSE_REGISTER(net_Read)
{


	if (msg["data"].str() == "eof")
	{
		INFO("GOT TO EOF FOR " << msg.dst);
		{
			boost::lock_guard<boost::mutex> lock(threadMutex);
			sessioneof++;
		}
		lastId[msg.dst]=-1;
		Cmsg out;
		out.event="net_Disconnect";
		out.src=msg.dst;
		out.dst=msg.src;
		out.send();
		return;
	}
	else
	//check if received value of this sessions against last one
	if (lastId[msg.dst]+1 != msg["data"])
	{
		ERROR("Data ordering error: Expected " << lastId[msg.dst]+1 << " but got " << msg["data"]);
		{
			boost::lock_guard<boost::mutex> lock(threadMutex);
			failed=true;
		}
	}
	lastId[msg.dst]=msg["data"];
	
}

SYNAPSE_REGISTER(net_Disconnected)
{
	boost::lock_guard<boost::mutex> lock(threadMutex);

	if (lastId[msg.dst]!=-1)
	{
		ERROR("Unexpected disconnect: lastId is not -1 but " << lastId[msg.src]);
	}
	sessions--;
	if (!sessions)
	{
		Cmsg out;
		if (failed)
		{
			ERROR("FAILED TESTS!");
			out["exit"]=1;
		}
		else
		if (sessioneof!=NET_SESSIONS*2)
		{
			ERROR("NOT ALL SESSIONS GOT TO EOF? expected " << NET_SESSIONS*2 << " but got " << sessioneof);
			out["exit"]=1;
		}
		else
		{
			INFO("LAST SESSION DISCONNECTED, ALL TESTS SUCCEEDED")
			Cmsg close;
			close.event="net_Close";
			close["port"]=12345;
			close.send();
		}
		out.event="core_Shutdown";
		out.send();
	}
	else
	{
		INFO("DISCONNECTED " << msg.dst << ", connections left: " << sessions);
	}
}




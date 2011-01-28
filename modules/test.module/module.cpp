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
int counter;
int counterSleep;

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=10;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=10;
	out.send();

	///tell the rest of the world we are ready for duty
	out.clear();
	out.event="core_Ready";
	out.send();


	//load the modules you wanna fiddle around with:
	out.clear();
	out.event="core_LoadModule";

//  	out["path"]="modules/asterisk.module/libasterisk.so";
//  	out.send();
//
//  	out["path"]="modules/pong.module/libpong.so";
//  	out.send();
//
//	out["path"]="modules/conn_json.module/libconn_json.so";
//	out.send();
//
//  	out["path"]="modules/marquee_m500.module/libmarquee_m500.so";
//  	out.send();
//
//  	out["path"]="modules/asterisk_marquee.module/libasterisk_marquee.so";
//  	out.send();
//
//  	out["path"]="modules/lirc.module/liblirc.so";
//	out.send();
//
//	out["path"]="modules/net.module/libnet.so";
//	out.send();
//
//  	out["path"]="modules/ami.module/libami.so";
//  	out.send();

//  	out["path"]="modules/paper.module/libpaper.so";
//  	out.send();
//
//
//	out["path"]="modules/http_json.module/libhttp_json.so";
//	out.send();

	out["path"]="modules/play_vlc.module/libplay_vlc.so";
	out.send();

//		out["path"]="modules/exec.module/libexec.so";
//		out.send();

	// Counter that ever counterSleep seconds emits a message.
	// Speed can be changed with appropriate messages
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"test_Counter";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"modules";
	out["recvGroup"]=	"anonymous";
	out.send();

	counter=0;
	counterSleep=10000000;
//	out.clear();
//	out.event="test_Counter";
//	out.send();


	//chat demo stuff
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"chat_Text";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"anonymous";
	out.send();

	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"chat_Poll";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"anonymous";
	out.send();

}

//TODO: make a regression test that verifies results.
SYNAPSE_REGISTER(exec_Ready)
{
	Cmsg out;
	out.dst=msg["session"];
	out.event="exec_Start";

	out["cmd"]="echo 'line1\\nline2\\nline3'";
	out["id"]="read test";
	out.send();

	out["cmd"]="exit 13";
	out["id"]="should return exit 13";
	out.send();

	out["cmd"]="kill $$";
	out["id"]="should return signal 15";
	out.send();

	out["cmd"]="/non/existing";
	out["id"]="should return exit 127";
	out.send();

	out["cmd"]="sleep 5";
	out["id"]="should take a while";
	out.send();

}

SYNAPSE_REGISTER(exec_Started)
{
	;
}

SYNAPSE_REGISTER(exec_Data)
{
	;
}

SYNAPSE_REGISTER(exec_Error)
{
	;
}

SYNAPSE_REGISTER(exec_Ended)
{
	;
}


std::string playUrl;
SYNAPSE_REGISTER(play_vlc_Ready)
{
	playUrl="http://listen.diXXX.fm/public3/chilloutdreams.pls";
	Cmsg out;
	out.dst=msg["session"];
	out.event="play_NewPlayer";
	out["description"]="second player instance";
	out.send();
}

//a new player has emerged
SYNAPSE_REGISTER(play_Player)
{
	Cmsg out;
	out.dst=msg.src;
	out.event="play_Open";
	out["url"]=playUrl;
	out.send();


	playUrl="/home/psy/mp3/01. Experience (1992)/sadfds";

}

SYNAPSE_REGISTER(play_Log)
{

}


SYNAPSE_REGISTER(play_InfoMeta)
{

}

SYNAPSE_REGISTER(test_Counter)
{
	usleep(counterSleep);
	counter++;
	Cmsg out;
	out.event="test_Counter";
	out["counter"]=counter;
	out["counterSleep"]=counterSleep;
	out.send();

}

void counterStatus()
{
	if (counterSleep<10000)
		counterSleep=10000;

	if (counterSleep>5000000)
		counterSleep=5000000;


	Cmsg out;
	out.event="test_CounterStatus";
	out["counterSleep"]=counterSleep;
	out.send();
}


SYNAPSE_REGISTER(test_CounterFaster)
{
	counterSleep=counterSleep-100000;
	counterStatus();
}

SYNAPSE_REGISTER(test_CounterSlower)
{
	counterSleep=counterSleep+100000;
	counterStatus();
}

SYNAPSE_REGISTER(test_CounterSpeed)
{
	counterSleep=msg["counterSleep"];
	counterStatus();
}



//SYNAPSE_REGISTER(http_json_Ready)
//{
//	Cmsg out;
//	out.clear();
//	out.event="http_json_Listen";
//	out["port"]="10080";
//	out.dst=msg["session"];
//	out.send();
//}



SYNAPSE_REGISTER(loop)
{

	Cmsg out;
	out.clear();
	out.event="loop";
	out["bla"]="loopback test";
	out.dst=msg.dst;
	out.send();
}


SYNAPSE_REGISTER(lirc_Ready)
{
	Cmsg out;
	out.clear();
	out.event="lirc_Connect";
	out["host"]="192.168.13.1";
	out["port"]="8765";
	out.send();


}


SYNAPSE_REGISTER(conn_json_Ready)
{
	Cmsg out;
	out.clear();
	out.event="conn_json_Listen";
	out["port"]="12345";
	out.dst=msg["session"];
	out.send();
}

SYNAPSE_REGISTER(net_Ready)
{
	Cmsg out;
	out.clear();
	out.event="net_Connect";
	out["host"]="192.168.13.1";
	out["port"]="8765";
	out.send();


}

SYNAPSE_REGISTER(net_Read)
{
}
SYNAPSE_REGISTER(lirc_Read)
{
}

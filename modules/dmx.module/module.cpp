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


#include "cnetman.h"
#include "cconfig.h"
#include "synapse.h"
#include <boost/regex.hpp>
#include <sstream>
#include <iomanip>
#include "exception/cexception.h"

using namespace boost;
using namespace std;

int moduleSessionId=0;
int netSessionId=0;

namespace dmx
{

synapse::Cconfig dmxValues;

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	moduleSessionId=msg.dst;

    dmxValues.load("etc/synapse/dmx.conf");


	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=10;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=1;
	out.send();

	out.clear();
	out.event="core_LoadModule";
  	out["name"]="http_json";
  	out.send();

    out.clear();
    out.event="core_LoadModule";
    out["name"]="timer";
    out.send();

	out.clear();
	out.event="core_NewSession";
  	out.send();

  	//anyone can set values
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"dmx_Set";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"modules";
	out.send();

	//anyone can set positions
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"dmx_SetPosition";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"modules";
	out.send();

	//anyone can receive updates
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"dmx_Update";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"modules";
	out["recvGroup"]=	"anonymous";
	out.send();

	//anyone can request full updates
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"dmx_Get";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"modules";
	out.send();

	//just connect something
	out.clear();
	out.event="dmx_Connect";
  	out["id"]=1;
  	out["host"]="192.168.1.6";
//  	out["host"]="localhost";
  	out["port"]=777;
  	out.send();


	///tell the rest of the world we are ready for duty
	out.clear();
	out.event="core_Ready";
	out.send();


}
 
class CnetDmx : public synapse::Cnet
{

 	void connected(int id, const string &host, int port)
	{
		Cmsg out;
		out.dst=id;
		out.event="dmx_Connected";
		out.send();

		//password
        string s("777\r\n");
        doWrite(s);

        //16bits mode
//        s="*65FF#\n";
   //     doWrite(s);
	}

	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		//convert streambuf to string
		string dataStr(boost::asio::buffer_cast<const char*>(readBuffer.data()), readBuffer.size());

		//parse lirc output
//		smatch what;
//		if (regex_match(
//			dataStr,
//			what,
//			boost::regex("(.*?) (.*?) (.*?) (.*?)\n")
//		))
//		{
//			//send to destination -1: this is the user configurable event mapper
//			//TODO: different events for long-presses and double presses?
//			Cmsg out;
//			out.dst=-1;
//			out.event="dmx_"+what[4]+"."+what[3];
//			out["code"]		=what[1];
//			out["repeat"]	=what[2];
//			out.send();
//		}
//		else
//		{
//			ERROR("Cant parse dmx output: " << dataStr);
//		}

		DEB("DMX answer: "<<dataStr);

		readBuffer.consume(dataStr.length());

	}

 	void disconnected(int id, const boost::system::error_code& ec)
	{
		Cmsg out;
		out.dst=id;
		out.event="dmx_Disconnected";
		out["reason"]=ec.message();
		out.send();
	}

 	void startAsyncRead()
 	{
 		asio::async_read_until(
 				tcpSocket,
 				readBuffer,
 				boost::regex("."),
 				bind(&Cnet::readHandler, this, _1, _2)
 			);
 	}

};

synapse::CnetMan<CnetDmx> net;

SYNAPSE_REGISTER(module_SessionStart)
{
	if (msg.dst!=moduleSessionId)
	{
		netSessionId=msg.dst;

		Cmsg out;
		out.clear();
		out.event="core_ChangeSession";
		out["maxThreads"]=10;
		out.send();
	}
}

SYNAPSE_REGISTER(timer_Ready)
{
    Cmsg out;
    out.event="timer_Set";
    out["seconds"]=10;
    out["repeat"]=-1;
    out["dst"]=dst;
    out["event"]="dmx_Timer";
    out.send();
}

SYNAPSE_REGISTER(dmx_Timer)
{
    if (dmxValues.isChanged())
        dmxValues.save();
}

SYNAPSE_REGISTER(dmx_Connect)
{
	if (msg.dst==netSessionId)
	{
		net.runConnect(msg["id"], msg["host"], msg["port"], 5);
	}
	else
	{
		msg.dst=netSessionId;
		msg.send();
	}
}

/** Set specified dmx channel to a value
 *
 */
SYNAPSE_REGISTER(dmx_Set)
{
	if (msg["channel"]>1024 || msg["channel"]<0)
		throw(synapse::runtime_error("Illegal channel"));

	if (msg["value"]>255 || msg["value"]<0)
		throw(synapse::runtime_error("Illegal value"));

//	if (msg["channel"]==6 && msg["value"]<180)
//		msg["value"]=180;


	stringstream dmxStr;
	//*C9<layer><channel><value>#
	dmxStr << "*C901";
	dmxStr << setfill('0');
	dmxStr << hex << setw(2) << (int)msg["channel"];
	dmxStr << hex << setw(2) << (int)msg["value"];
	dmxStr << "#\n";
	string s=dmxStr.str();
	net.doWrite(msg["id"], s);

	Cmsg out;
	out=msg;
	out.event="dmx_Update";
	out.src=0;
	out.dst=0;
	out.send();

	dmxValues[msg["channel"]]["value"]=msg["value"];
    dmxValues.changed();
}


/** Set dmx position
 *
 */
SYNAPSE_REGISTER(dmx_SetPosition)
{
	if (msg["channel"]>1024 || msg["channel"]<0)
		throw(synapse::runtime_error("Illegal channel"));

	dmxValues[msg["channel"]]["left"]=msg["left"];
	dmxValues[msg["channel"]]["top"]=msg["top"];

	Cmsg out;
	out=msg;
	out.event="dmx_Update";
	out.src=0;
	out.dst=0;
	out.send();

    dmxValues.changed();
}

SYNAPSE_REGISTER(dmx_Get)
{
	FOREACH_VARMAP(value, dmxValues)
	{
		Cmsg out;
		out.dst=msg.src;
		out.event="dmx_Update";
		out=value.second;
		out["channel"]=value.first;
		out.send();

	}

}

SYNAPSE_REGISTER(dmx_Disconnect)
{
	net.doDisconnect(msg["id"]);
}

/** When a session ends, make sure the corresponding network connection is disconnected as well.
 *
 */
SYNAPSE_REGISTER(module_SessionEnded)
{
	net.doDisconnect(msg["id"]);
}

/** Called when synapse whats the module to shutdown completely
 * This makes sure that all ports and network connections are closed, so there wont be any 'hanging' threads left.
 * If you care about data-ordering, send this to session-id that sended you the net_Connected.
 */
SYNAPSE_REGISTER(module_Shutdown)
{
	INFO("dmx shutting down...");
	//let the net module shut down to fix the rest
	net.doShutdown();
}
}
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
#include "synapse.h"
#include <boost/regex.hpp>

using namespace boost;
using namespace std;

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
}
 
class CnetLirc : public synapse::Cnet
{

 	void connected(int id, const string &host, int port)
	{
		Cmsg out;
		out.dst=id;
		out.event="lirc_Connected";
		out.send();
	}

	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		//convert streambuf to string
		string dataStr(boost::asio::buffer_cast<const char*>(readBuffer.data()), readBuffer.size());
		dataStr.resize(dataStr.find(delimiter)+delimiter.length());

		
		/* Example lirc output:
			0000000000001010 00 sys_00_command_10 PHILIPS_RC-5
			0000000000001010 01 sys_00_command_10 PHILIPS_RC-5
			0000000000001010 02 sys_00_command_10 PHILIPS_RC-5
			0000000000001010 03 sys_00_command_10 PHILIPS_RC-5
			0000000000001011 00 sys_00_command_11 PHILIPS_RC-5
			0000000000001011 01 sys_00_command_11 PHILIPS_RC-5
			0000000000001011 02 sys_00_command_11 PHILIPS_RC-5
		*/
		//parse lirc output
		smatch what;
		if (regex_match(
			dataStr,
			what, 
			boost::regex("(.*?) (.*?) (.*?) (.*?)\n")
		))
		{
			//send to destination -1: this is the user configurable event mapper
			//TODO: different events for long-presses and double presses?
			Cmsg out;
			out.dst=-1;
			out.event="lirc_"+what[4]+"."+what[3];
			out["code"]		=what[1];
			out["repeat"]	=what[2];
			out.send();
		}
		else
		{
			ERROR("Cant parse lirc output: " << dataStr);
		}

		readBuffer.consume(dataStr.length());

	}

 	void disconnected(int id, const boost::system::error_code& ec)
	{
		Cmsg out;
		out.dst=id;
		out.event="lirc_Disconnected";
		out["reason"]=ec.message();
		out.send();
	}


};

synapse::CnetMan<CnetLirc> net;

SYNAPSE_REGISTER(lirc_Connect)
{
	net.runConnect(msg.src, msg["host"], msg["port"], 5);
}

SYNAPSE_REGISTER(lirc_Disconnect)
{
	net.doDisconnect(msg.src);
}

/** When a session ends, make sure the corresponding network connection is disconnected as well.
 *
 */
SYNAPSE_REGISTER(module_SessionEnded)
{
	net.doDisconnect(msg["session"]);
}

/** Called when synapse whats the module to shutdown completely
 * This makes sure that all ports and network connections are closed, so there wont be any 'hanging' threads left.
 * If you care about data-ordering, send this to session-id that sended you the net_Connected.
 */
SYNAPSE_REGISTER(module_Shutdown)
{
	INFO("conn_json shutting down...");
	//let the net module shut down to fix the rest
	net.doShutdown();
}

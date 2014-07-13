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



/** arduino module for my arduino home automation project

 arduino code at: https://github.com/psy0rz/stuff/tree/master/rfid

*/


#include <string>

#include "synapse.h"
#include "cserial.h"
#include <boost/regex.hpp>
#include <time.h>
#include <cconfig.h>

using namespace boost;
using namespace std;

int moduleSessionId=0;

synapse::Cconfig state;
synapse::Cconfig db;

/** module_Init - called first, set up basic stuff here
 */
SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;
	moduleSessionId=msg.dst;

	state.load("var/arduino.state");
	db.load("var/arduino.db");

	//one read tread (cserial run()), and one thread to receive writes.
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=2;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=2;
	out.send();

	//receive by anonymous only:
	out.clear();
	out.event="core_ChangeEvent";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"modules";
	out["recvGroup"]=	"anonymous";
 	out["event"]=		"arduino_Received"; 
 	out.send();

	//send by anonymous only:
	out.clear();
	out.event="core_ChangeEvent";
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"modules";
	out["event"]=		"asterisk_Send";
	out.send();

}

class CserialModule : public synapse::Cserial
{
	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		Cmsg out;
		string dataStr(boost::asio::buffer_cast<const char*>(readBuffer.data()), readBuffer.size());
		dataStr.resize(dataStr.find(delimiter)+delimiter.length());
		readBuffer.consume(dataStr.length());


		//convert into nice synapse event
		smatch what;
		if (regex_search(
			dataStr,
			what, 
			boost::regex("([0-9]*) ([^ ]*) (.*)\r")
		))
		{
			out.event="arduino_Received";
			out["node"]=what[1];
			out["event"]=what[2];
			out["data"]=what[3];
			out["time"]=time(NULL);

			//store events per node in a tree
			state[out["node"]][out["event"]]["data"]=out["data"];
			state[out["node"]][out["event"]]["time"]=out["time"];
			state.changed();
			state.save();

		}
		else
		{
			ERROR("arduino: Unknown data received: " << dataStr);
		}
		out.dst=0;
		out.send();

	}

};

CserialModule serial;


SYNAPSE_REGISTER(module_SessionStart)
{

	///tell the rest of the world we are ready for duty
	Cmsg out;
	out.event="core_Ready";
	out.send();

	using namespace boost::asio;
	serial.doOpen(0, "\n", "/dev/ttyUSB0", 
		serial_port_base::baud_rate(9600), 
		serial_port_base::character_size(8),
		serial_port_base::parity(serial_port_base::parity::none), 
		serial_port_base::stop_bits(serial_port_base::stop_bits::one), 
		serial_port_base::flow_control(serial_port_base::flow_control::none)
	); 
	serial.run();

}

/** Re-send all arduino events to caller */
SYNAPSE_REGISTER(arduino_Refresh)
{
	FOREACH_VARMAP_ITER(nodeI,state)
	{
		Cmsg out;
		out.event="arduino_Received";
		out.dst=msg.src;
		out["node"]=nodeI->first;
		FOREACH_VARMAP_ITER(eventI,nodeI->second)
		{
			out["event"]=eventI->first;
			out["data"]=eventI->second["data"];
			out["time"]=eventI->second["time"];
			out.send();
		}
	}
}


SYNAPSE_REGISTER(arduino_Send)
{
	string s;
	s=msg["node"].str()+" "+msg["event"].str()+" "+msg["data"].str()+"\n";
	serial.doWrite(s);
}

/** Store stuff in database (settings and so on)
* table: table/category
* name: name and identifier of the data object (overwrites existing)
* data: actual data (if not set will delete the data)
*/
SYNAPSE_REGISTER(arduino_DbPut)
{
	if (msg.isSet("data"))
	{
		db[msg["table"]][msg["name"]]=msg["data"];
	}
	//delete
	else
	{
		db[msg["table"]].map().erase(msg["name"]);
	}
	db.changed();
	db.save();
}

/** Gets stuff from database by sending arduino_DbItem or arduino_DbItems (depending if name was specified or not)
*/
SYNAPSE_REGISTER(arduino_DbGet)
{
	if (msg.isSet("name"))
	{
		Cmsg out;
		out.dst=msg.src;
		out.event="arduino_DbItem";
		out["table"]=msg["table"];
		out["name"]=msg["name"];
		out["data"]=db[msg["table"]][msg["name"]];
		out["request_id"]=msg["request_id"];
		out.send();
	}
	else
	{
		Cmsg out;
		out.dst=msg.src;
		out.event="arduino_DbItems";
		out["table"]=msg["table"];
		out["items"].list();
		out["request_id"]=msg["request_id"];
		FOREACH_VARMAP(item, db[msg["table"]])
		{
			out["items"].list().push_back(item.first);
		}
		out.send();
	}
}


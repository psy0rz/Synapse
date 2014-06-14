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



using namespace boost;
using namespace std;

int moduleSessionId=0;
/** module_Init - called first, set up basic stuff here
 */
SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;
	moduleSessionId=msg.dst;

	//one read tread (cserial run()), and one thread to receive writes.

	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=2;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=2;
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
		serial_port_base::baud_rate(115200), 
		serial_port_base::character_size(8),
		serial_port_base::parity(serial_port_base::parity::none), 
		serial_port_base::stop_bits(serial_port_base::stop_bits::one), 
		serial_port_base::flow_control(serial_port_base::flow_control::none)
	); 
	serial.run();

}




/** Write data to the connection related to src
 * If you care about data-ordering, send this to session-id that sended you the net_Connected.
 */
SYNAPSE_REGISTER(net_Write)
{
	//linebased, so add a newline
	msg["data"].str()+="\n";
	serial.doWrite(msg["data"]);
}

/** When a session ends, make sure the corresponding network connection is disconnected as well.
 *
 */
SYNAPSE_REGISTER(module_SessionEnded)
{
	serial.doClose();
}


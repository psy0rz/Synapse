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



#include "cserial.h"
#include "clog.h"

//using namespace std;
//using namespace boost;
//using asio::ip::tcp;
namespace synapse
{

using namespace std;
using namespace boost;

Cserial::Cserial()
		readBuffer(65535), serialPort(ioService) 
{
	delimiter="\n";
} 



void Cserial::doOpen(int id, string delimiter, string port, 
	boost::asio::serial_port_base::baud_rate baud_value,
	boost::asio::serial_port_base::flow_control flow_control_value,
	boost::asio::serial_port_base::parity parity_value,
	boost::asio::serial_port_base::stop_bits stop_bits_value,
	boost::asio::serial_port_base::character_size character_size_value)
{
	this->id=id;
	this->delimiter=delimiter;
	serialPort.open(port);
	serialPort.set_option(baud_value);
	serialPort.set_option(flow_control_value);
	serialPort.set_option(parity_value);
	serialPort.set_option(stop_bits_value);
	serialPort.set_option(character_size_value);
	//start reading the next incoming data
	startAsyncRead();
}


Cserial::~Cserial ()
{
	DEB("serial object " << id << " destroyed");
}


//handle the results of a read (incoming data) 
void Cserial::readHandler(
	const boost::system::error_code& ec,
	std::size_t bytesTransferred)
{
	if (ec)
	{
		reset(ec);
		return;
	}
	
	//netMan.read(id, readBuffer, bytesTransferred);
	received(id, readBuffer, bytesTransferred);

	//start reading the next incoming data
	startAsyncRead();

}

void Cserial::closeHandler()
{
	DEB("Initiating port close for id " << id);
	serialPort.close();
}



void Cserial::doClose()
{
	ioService.post(bind(&Cserial::closeHandler,this));
}

void Cserial:doWrite(string & data)
{
	//copy string into a buffer and pass it to the ioService
	shared_ptr<string> stringPtr(new string(data));
	asio::async_write(
		tcpSocket, 
		asio::buffer(*stringPtr), 
		bind(&Cnet::writeStringHandler, this, stringPtr, _1, _2)
	);

}

void Cnet::doWrite(shared_ptr<asio::streambuf> bufferPtr)
{
	asio::async_write(
		tcpSocket, 
		*bufferPtr, 
		bind(&Cnet::writeStreambufHandler, this, bufferPtr, _1, _2)
	);

}

void Cnet::writeStringHandler(
	shared_ptr<string> stringPtr,
	const boost::system::error_code& ec, 
	std::size_t bytesTransferred)
{
	if (ec)
	{
		reset(ec);
		return;
	}

}

void Cnet::writeStreambufHandler(
	shared_ptr<asio::streambuf> bufferPtr,
	const boost::system::error_code& ec, 
	std::size_t bytesTransferred)
{
	if (ec)
	{
		reset(ec);
		return;
	}

}

void Cnet::run()
{
	ioService.run();
}

void Cnet::reset(const boost::system::error_code& ec)
{
	//we probably got disconnected or had some kind of error,
	//just cancel and close everything to be sure.
	connectTimer.cancel();
	tcpResolver.cancel();
	tcpSocket.close();

	//callback for user
	disconnected(id, ec);

	//check if we need to reconnect
	if (reconnectTime)
	{
		DEB("Will reconnect id " << id << ", after " << reconnectTime << " seconds...");
		sleep(reconnectTime);
		doConnect();
	}

}

void Cnet::startAsyncRead()
{
	asio::async_read_until(
		tcpSocket,
		readBuffer,
		delimiter,
		bind(&Cnet::readHandler, this, _1, _2)
	);
}


void Cnet::init(int id)
{
	//dummy
	DEB(id << " initalizing");
}

void Cnet::init_server(int id, CacceptorPtr acceptorPtr)
{
	DEB(id << " initalizing server");
}



void Cnet::accepting(int id, int port)
{
	//dummy
	DEB(id << " is accepting connection on " << port);
}



void Cnet::connecting(int id, const string &host, int port)
{
	//dummy
	DEB(id << " is connecting to " << host << ":" << port);
}

void Cnet::connected(int id, const string &host, int port)
{
	//dummy
}

void Cnet::connected_server(int id, const string &host, int port, int local_port)
{
	//dummy
	DEB(id << " server is connected to " << host << ":" << port);
}

void Cnet::connected_client(int id, const string &host, int port)
{
	//dummy
	DEB(id << " client is connected to " << host << ":" << port);
}




void Cnet::disconnected(int id, const boost::system::error_code& error)
{
	//dummy
	DEB(id << " has been disconnected:" << error.message());
}



void Cnet::received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
{
	//dummy
	DEB(id << " has received data:" << &readBuffer);
	readBuffer.consume(bytesTransferred);
}


void Cnet::getStatus(Cvar & var)
{
	if (tcpSocket.is_open())
	{
		var["id"]=id;
		var["localAddr"]=tcpSocket.local_endpoint().address().to_string();
		var["localPort"]=tcpSocket.local_endpoint().port();
		var["remoteAddr"]=tcpSocket.remote_endpoint().address().to_string();
		var["remotePort"]=tcpSocket.remote_endpoint().port();
	}
}

}

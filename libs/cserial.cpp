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
		:readBuffer(65535), serialPort(ioService) 
{
	delimiter="\n";
} 



void Cserial::doOpen(int id, string delimiter, string port, 
	boost::asio::serial_port_base::baud_rate baud_value,
	boost::asio::serial_port_base::character_size character_size_value,
	boost::asio::serial_port_base::parity parity_value,
	boost::asio::serial_port_base::stop_bits stop_bits_value,
	boost::asio::serial_port_base::flow_control flow_control_value)
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

void Cserial::doWrite(string & data)
{
	shared_ptr<asio::streambuf> bufferPtr(new asio::streambuf(data.size()));
	std::ostream os(&*bufferPtr);
	os << data;
	doWrite(bufferPtr);
}

void Cserial::doWrite(shared_ptr< asio::streambuf> bufferPtr)
{
	//post data to the service-thread
	ioService.post(bind(&Cserial::writeHandler,this, bufferPtr));
}


//executed from io-service thread
void Cserial::writeHandler(shared_ptr< asio::streambuf> bufferPtr)
{
	if (writeQueue.empty())
	{		
		writeQueue.push_back(bufferPtr);
		//if its empty it means we're not currently writing stuff, so we should start a new async write.
		asio::async_write(
			serialPort, 
			*writeQueue.front(),
			bind(&Cserial::writeCompleteHandler, this, _1, _2)
		);
	}
	else
	{
		writeQueue.push_back(bufferPtr);
	}
};

//boost only allows us to call async_write ONCE and then we have to wait for its completion. thats why we need the writequeue.)
//the front item on the queue is the on being currently processed by async_write.
//executed from io-service thread
void Cserial::writeCompleteHandler(
	const boost::system::error_code& ec, 
	std::size_t bytesTransferred)
{
	//remove the front item, since the write is complete now. because of the shared_ptrs it will be freed automaticly.
	if (!writeQueue.empty())
		writeQueue.pop_front();

	//error?
	if (ec)
	{
		reset(ec);
		return;
	}

	if (!writeQueue.empty())
	{
		//start next async write.
		asio::async_write(
			serialPort, 
			*writeQueue.front(),
			bind(&Cserial::writeCompleteHandler, this, _1, _2)
		);
	}
}




//executed from io-service thread
void Cserial::run()
{
	ioService.run();
}

//executed from io-service thread
void Cserial::reset(const boost::system::error_code& ec)
{
	//callback for user
	error(id, ec);

}

//executed from io-service thread
void Cserial::startAsyncRead()
{
	asio::async_read_until(
		serialPort,
		readBuffer,
		delimiter,
		bind(&Cserial::readHandler, this, _1, _2)
	);
}



//executed from io-service thread
void Cserial::error(int id, const boost::system::error_code& error)
{
	//dummy
	DEB(id << " had an error:" << error.message());
}



//executed from io-service thread
void Cserial::received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
{
	//dummy
	DEB(id << " has received data:" << &readBuffer);
	readBuffer.consume(bytesTransferred);
}



}

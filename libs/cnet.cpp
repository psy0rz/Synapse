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


// Dont use directly: Use CnetMan instead. Look at the network-module example how to use.

#include "cnet.h"
#include "clog.h"
//#include <boost/asio.hpp>

//using namespace std;
//using namespace boost;
//using asio::ip::tcp;
namespace synapse
{

using namespace std;
using namespace boost;
using asio::ip::tcp;

Cnet::Cnet()
		:tcpResolver(ioService), connectTimer(ioService), readBuffer(65535), tcpSocket(ioService)
{
	delimiter="\n";
}

//server mode: accept a new connection from the specified acceptorPtr. (usually provided by CnetMan)
void Cnet::doAccept(int id, CacceptorPtr acceptorPtr)
{
	this->id=id;

	init_server(id, acceptorPtr);
	init(id);

	reconnectTime=0;
	DEB("Starting acceptor for port " << acceptorPtr->local_endpoint().port()<< " into id " << id);
	statusServer=acceptorPtr->local_endpoint().port();

	//start the accept
	asio::io_service::work work(ioService);
	acceptorPtr->async_accept(
		tcpSocket,
		bind(&Cnet::acceptHandler,this,asio::io_service::work(ioService),_1)
	);

	accepting(id, acceptorPtr->local_endpoint().port());

}


//Client mode: Initiate a connect.
void Cnet::doConnect(int id, string host, int port, int reconnectTime, string delimiter)
{
	this->id=id;
	this->host=host;
	this->port=port;
	this->reconnectTime=reconnectTime;
	this->delimiter=delimiter;
	doConnect();
}

void Cnet::doConnect()
{
	init(id);


	//in case of a reconnect we need some extra cleaning
	tcpResolver.cancel();
	tcpSocket.close();


	//start the resolver
	stringstream portStr;
	portStr << port;
	DEB("Starting resolver for id " << id << ", resolving: " << host<<":"<<port);
	tcp::resolver::query connectQuery(host, portStr.str());
	tcpResolver.async_resolve(connectQuery,
			bind(&Cnet::resolveHandler,this, _1 ,_2)
	);

	//start the timerout timer
	connectTimer.expires_from_now(boost::posix_time::seconds(CONNECT_TIMEOUT));
	connectTimer.async_wait(boost::bind(&Cnet::connectTimerHandler, this,_1));

	connecting(id, host, port);

}



//a connect timeout happend
//executed from io-service thread
void Cnet::connectTimerHandler(
	const boost::system::error_code& ec
)
{
	if (!ec)
	{
		//A connect-timeout happend, so cancel all operations.
		//the other handlers will reconnect if neccesary
		tcpResolver.cancel();
		tcpSocket.close();
	}
}



//executed from io-service thread
void Cnet::acceptHandler(
	asio::io_service::work work,
	const boost::system::error_code& ec
)
{
	if (ec)
	{
		DEB("Accepting for id " << id << " failed: " <<ec.message());
		reset(ec);
		return;
	}

	//when an acception has succeeded, we also use the connected-callback.
	//this ways its easy to change existing code between server and client mode.
	//we ALSO call a specific server handler, just in case the user needs the distinction
	connected(       id, tcpSocket.remote_endpoint().address().to_string(), tcpSocket.remote_endpoint().port());
	connected_server(id, tcpSocket.remote_endpoint().address().to_string(), tcpSocket.remote_endpoint().port(), tcpSocket.local_endpoint().port());

	//start reading the incoming data
	startAsyncRead();

}

Cnet::~Cnet ()
{
	DEB("net object " << id << " destroyed");
}



//handle the results of the resolver and start connecting to the first endpoint:
//executed from io-service thread
void Cnet::resolveHandler(
	const boost::system::error_code& ec,
	asio::ip::tcp::resolver::iterator endpointI)
{
	if (ec)
	{
		DEB("Resolver for id " << id << " failed: " <<ec.message());
		reset(ec);
		return;
	}

	//start connecting to first endpoint:
	tcp::endpoint endpoint = *endpointI;
	DEB("Resolved id " << id << ", starting connect to " << endpoint.address() << ":" << endpoint.port());
	tcpSocket.async_connect(endpoint,
			bind(&Cnet::connectHandler, this, ++endpointI, _1)
	);
}

//handle the results of the connect:
//-try connecting to the next endpoints in case of failure
//-start waiting for data in case of succes
//executed from io-service thread
void Cnet::connectHandler(
	asio::ip::tcp::resolver::iterator endpointI,
	const boost::system::error_code& ec
	)
{
	if (ec)
	{
		//we still have other endpoints we can try to connect?
		if (endpointI != tcp::resolver::iterator())
		{
			tcpSocket.close();
			tcp::endpoint endpoint = *endpointI;
			DEB("Connection for id " << id << " failed, trying next endpoint: " << endpoint.address());
			tcpSocket.async_connect(endpoint,
					bind(&Cnet::connectHandler, this, ++endpointI, _1)
			);
		}
		//failure
		else
		{
			reset(ec);
		}
		return;
	}

	//connection succeeded
	connectTimer.cancel();
	connected(       id, tcpSocket.remote_endpoint().address().to_string(), tcpSocket.remote_endpoint().port());
	connected_client(id, tcpSocket.remote_endpoint().address().to_string(), tcpSocket.remote_endpoint().port());


	//start reading the incoming data
	startAsyncRead();


}

//handle the results of a read (incoming data)
//used for both client and servers.
//executed from io-service thread
void Cnet::readHandler(
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

//executed from io-service thread
void Cnet::disconnectHandler()
{
	DEB("Initiating permanent disconnect for id " << id);
	reconnectTime=0;
	tcpResolver.cancel();
	tcpSocket.close();
}



void Cnet::doDisconnect()
{
	ioService.post(bind(&Cnet::disconnectHandler,this));
}


void Cnet::doWrite(string & data)
{
	boost::shared_ptr<asio::streambuf> bufferPtr(new asio::streambuf(data.size()));
	std::ostream os(&*bufferPtr);
	os << data;
	doWrite(bufferPtr);
}

void Cnet::doWrite(boost::shared_ptr< asio::streambuf> bufferPtr)
{
	//post data to the service-thread
	ioService.post(bind(&Cnet::writeHandler,this, bufferPtr));
}

//executed from io-service thread
void Cnet::writeHandler(boost::shared_ptr< asio::streambuf> bufferPtr)
{
	if (writeQueue.empty())
	{
		writeQueue.push_back(bufferPtr);
		//if its empty it means we're not currently writing stuff, so we should start a new async write.
		asio::async_write(
			tcpSocket,
			*writeQueue.front(),
			bind(&Cnet::writeCompleteHandler, this, _1, _2)
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
void Cnet::writeCompleteHandler(
	const boost::system::error_code& ec,
	std::size_t bytesTransferred)
{
	//remove the front item, since the write is complete now. because of the boost::shared_ptrs it will be freed automaticly.
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
			tcpSocket,
			*writeQueue.front(),
			bind(&Cnet::writeCompleteHandler, this, _1, _2)
		);
	}
}


//executed from io-service thread
void Cnet::run()
{
	ioService.run();
}

//executed from io-service thread
void Cnet::reset(const boost::system::error_code& ec)
{
	//we probably got disconnected or had some kind of error,
	//just cancel and close everything to be sure.
	connectTimer.cancel();
	tcpResolver.cancel();
	tcpSocket.close();

	//callback for user
	disconnected(id, ec);

	writeQueue.clear();

	//check if we need to reconnect
	if (reconnectTime)
	{
		DEB("Will reconnect id " << id << ", after " << reconnectTime << " seconds...");
		sleep(reconnectTime);
		doConnect();
	}

}

//executed from io-service thread
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

//executed from io-service thread
void Cnet::connected(int id, const string &host, int port)
{
	//dummy
}

//executed from io-service thread
void Cnet::connected_server(int id, const string &host, int port, int local_port)
{
	//dummy
	DEB(id << " server is connected to " << host << ":" << port);
}

//executed from io-service thread
void Cnet::connected_client(int id, const string &host, int port)
{
	//dummy
	DEB(id << " client is connected to " << host << ":" << port);
}



//executed from io-service thread
void Cnet::disconnected(int id, const boost::system::error_code& error)
{
	//dummy
	DEB(id << " has been disconnected:" << error.message());
}



//executed from io-service thread
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

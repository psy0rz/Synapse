//
// C++ Implementation: cnet
//
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "cnet.h"
#include "clog.h"

//client mode: construct the net-object and start resolving immeadiatly
Cnet::Cnet(CnetMan & netManRef, int id, string host, int port, int reconnectTime)
		:netMan(netManRef), tcpSocket(ioService), tcpResolver(ioService), readBuffer(65535), connectTimer(ioService)
{
	this->id=id;
	this->host=host;
	this->port=port;
	this->reconnectTime=reconnectTime;

	doConnect();
} 

//Used in client mode to actually do the connect-stuff
void Cnet::doConnect()
{
	//in case of a reconnect we need some extra cleaning
	tcpResolver.cancel();
	tcpSocket.close();

	stringstream portStr;
	portStr << port;
	ioService.post(bind(&CnetMan::connecting,&netMan,id,host,port));

	//start the resolver	
	DEB("Starting resolver for id " << id << ", resolving: " << host<<":"<<port);
	tcp::resolver::query connectQuery(host, portStr.str());
	tcpResolver.async_resolve(connectQuery,
			bind(&Cnet::resolveHandler,this, _1 ,_2)
	);

	//start the timerout timer
	connectTimer.expires_from_now(boost::posix_time::seconds(CONNECT_TIMEOUT));
    connectTimer.async_wait(boost::bind(&Cnet::connectTimerHandler, this,_1));
    
}

//a connect timeout happend
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

//Called on disconnect: Checks if we need to reconnect.
void Cnet::doReconnect()
{
	if (reconnectTime)
	{
		DEB("Will reconnect id " << id << ", after " << reconnectTime << " seconds...");
		sleep(reconnectTime);
		doConnect();
	}
}

//server mode: construct the net-object from a new connection that comes from the acceptor.
Cnet::Cnet(CnetMan & netManRef, int id, CacceptorPtr acceptorPtr)
		:netMan(netManRef), tcpSocket(ioService), tcpResolver(ioService), readBuffer(65535), connectTimer(ioService)
{
	this->id=id;

	//inform the world we're trying to accept a new connection
	ioService.post(bind(
		&CnetMan::accepting,
		&netMan,
		acceptorPtr->local_endpoint().port(),
		id)
	);

	reconnectTime=0;
	DEB("Starting acceptor for port " << acceptorPtr->local_endpoint().port()<< " into id " << id);
	//start the accept

	asio::io_service::work work(ioService);
	acceptorPtr->async_accept(
		tcpSocket,
		bind(&Cnet::acceptHandler,this,asio::io_service::work(ioService),_1)
	);	
		
} 

void Cnet::acceptHandler(
	asio::io_service::work work,
	const boost::system::error_code& ec
)
{
	if (ec)
	{
		DEB("Accepting for id " << id << " failed: " <<ec.message());
		disconnected(ec);
		return;
	}

	//when an acception has succeeded, we also use the connected-callback.
	//this ways its easy to change existing code between server and client mode.
	netMan.connected(id);

	//start reading the incoming data
	asio::async_read_until(tcpSocket,
		readBuffer,
		"\n",
		bind(&Cnet::readHandler, this, _1, _2)
	);

}

Cnet::~Cnet ()
{
	DEB("net object " << id << " destroyed");
}



//handle the results of the resolver and start connecting to the first endpoint:
void Cnet::resolveHandler(
	const boost::system::error_code& ec,
	asio::ip::tcp::resolver::iterator endpointI)
{
	if (ec)
	{
		DEB("Resolver for id " << id << " failed: " <<ec.message());
		disconnected(ec);
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
			disconnected(ec);
		}
		return;
	}

	//connection succeeded
	connectTimer.cancel();
	netMan.connected(id);

	//start reading the incoming data
	asio::async_read_until(tcpSocket,
		readBuffer,
		"\n",
		bind(&Cnet::readHandler, this, _1, _2)
	);


}

//handle the results of a read (incoming data) 
void Cnet::readHandler(
	const boost::system::error_code& ec,
	std::size_t bytesTransferred)
{
	if (ec)
	{
		disconnected(ec);
		return;
	}
	
	netMan.read(id, readBuffer, bytesTransferred);
	readBuffer.consume(bytesTransferred);

	//start reading the next incoming data
	asio::async_read_until(tcpSocket,
		readBuffer,
		"\n",
		bind(&Cnet::readHandler, this, _1, _2)
	);

}

void Cnet::doDisconnectHandler()
{
	DEB("Initiating permanent disconnect for id " << id);
	reconnectTime=0;
	tcpResolver.cancel();
	tcpSocket.close();
}

void Cnet::writeHandler(
	shared_ptr<string> stringPtr,
	const boost::system::error_code& ec, 
	std::size_t bytesTransferred)
{
	if (ec)
	{
		disconnected(ec);
		return;
	}

	//no use to call this yet?
	//netMan.wrote(id, stringPtr);

}


void Cnet::doDisconnect()
{
	ioService.post(bind(&Cnet::doDisconnectHandler,this));
}

void Cnet::doWrite(string & data)
{
	//copy string into a buffer and pass it to the ioService
	shared_ptr<string> stringPtr(new string(data));
	asio::async_write(
		tcpSocket, 
		asio::buffer(*stringPtr), 
		bind(&Cnet::writeHandler, this, stringPtr, _1, _2)
	);

}

void Cnet::run()
{
	ioService.run();
}

void Cnet::disconnected(const boost::system::error_code& ec)
{
	//we probably got disconnected or had some kind of error,
	//just cancel and close everything to be sure.
	connectTimer.cancel();
	tcpResolver.cancel();
	tcpSocket.close();

	//callback to netmanager
	netMan.disconnected(id, ec);

	//check if we need to reconnect
	doReconnect();
}


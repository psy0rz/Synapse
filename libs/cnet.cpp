// Dont use directly: Use CnetMan instead. Look at the network-module example how to use.

#include "cnet.h"
#include "clog.h"

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

	//start the accept
	asio::io_service::work work(ioService);
	acceptorPtr->async_accept(
		tcpSocket,
		bind(&Cnet::acceptHandler,this,asio::io_service::work(ioService),_1)
	);	

	accepting(id, acceptorPtr->local_endpoint().port());
		
} 


//Client mode: Initiate a connect.
void Cnet::doConnect(int id, string host, int port, int reconnectTime)
{
	this->id=id;
	this->host=host;
	this->port=port;
	this->reconnectTime=reconnectTime;
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
	connected_server(id, tcpSocket.remote_endpoint().address().to_string(), tcpSocket.remote_endpoint().port());

	//start reading the incoming data
	startAsyncRead();

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

void Cnet::disconnectHandler()
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
		reset(ec);
		return;
	}

}


void Cnet::doDisconnect()
{
	ioService.post(bind(&Cnet::disconnectHandler,this));
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

void Cnet::connected_server(int id, const string &host, int port)
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



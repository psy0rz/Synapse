//
// C++ Interface: cnet
//
// Description: the Cnet class that handles a networking session. Its used to implement both servers and clients. 
// Threadsafe because we only post stuff with io_serivce::post()
// The CnetMan class is responsible for handling the creation of these Cnet-objects.
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#ifndef CNET_H
#define CNET_H

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <string>
#include <boost/shared_ptr.hpp>
/*





*/

using namespace std;
using namespace boost;
using asio::ip::tcp;

#define CONNECT_TIMEOUT 60

typedef shared_ptr<tcp::acceptor> CacceptorPtr;


class Cnet
{
	
	public:

	Cnet();
	~Cnet();

	//end-user api to ask us to DO stuff: (usually called from CnetMan)
	void doDisconnect();
	void doAccept(int id, CacceptorPtr acceptorPtr);
	void doConnect(int id, string host, int port, int reconnectTime=0);
	void doWrite(string & data);	
	void run();


	//this needs to be public, in case someone overrides startAsyncRead():
	void readHandler(
		const boost::system::error_code& ec,
		std::size_t bytesTransferred);
	
	private:
	int id;
	asio::io_service ioService;
	int reconnectTime;

	tcp::resolver tcpResolver;
	asio::deadline_timer connectTimer;


	string host;
	int port;

	protected:

	//change these settings in the init() callback handler.
	string delimiter;

	//this needs to be protected, in case someone overrides startAsyncRead():
	asio::streambuf readBuffer; 
	tcp::socket tcpSocket;

	private:

	//Internal functions
	void doConnect();


	//ASIO handlers to handle asynconious asio events:
	void acceptHandler(
		asio::io_service::work work,
		const boost::system::error_code& ec
	);

	void resolveHandler(
		const boost::system::error_code& ec,
		asio::ip::tcp::resolver::iterator endpointI);

	void connectHandler(
		asio::ip::tcp::resolver::iterator endpointI,
		const boost::system::error_code& ec
		);


	void writeHandler(
		shared_ptr<string> stringPtr,
		const boost::system::error_code& ec, 
		std::size_t bytesTransferred);

	void connectTimerHandler(
		const boost::system::error_code& ec
	);

	void disconnectHandler();

	void reset(const boost::system::error_code& ec);

	//you might want to override this if you need more flexible reading.
	//(usefull for http servers)
	virtual void startAsyncRead();

	//end-user "hooks" for server
	virtual void init_server(int id, CacceptorPtr acceptorPtr);
	virtual void accepting(int id, int port);
	virtual void connected_server(int id, const string &host, int port);
 
	//end-user "hooks" for client
	virtual void connecting(int id, const string &host, int port);
	virtual void connected_client(int id, const string &host, int port);

	//end-user "hooks" for client and server 
	virtual void init(int id);
	virtual void connected(int id, const string &host, int port);
	virtual void disconnected(int id, const boost::system::error_code& error);
	virtual void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred);

};



#endif

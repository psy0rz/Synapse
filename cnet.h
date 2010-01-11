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

typedef shared_ptr<class Cnet> CnetPtr;
typedef shared_ptr<tcp::acceptor> CacceptorPtr;
#include "cnetman.h"

class CnetMan;

class Cnet
{
	public:

	Cnet(CnetMan & netManRef, int id, string host, int port, int reconnectTime=0);
	Cnet(CnetMan & netManRef, int id, CacceptorPtr acceptorPtr);
	virtual ~Cnet();

	void doDisconnect();
	void doConnect();
	void doReconnect();
	void doWrite(string & data);	
	void run();
	
	private:
	int id;
	CnetMan &netMan;
	asio::io_service ioService;
	int reconnectTime;

	tcp::socket tcpSocket;
	tcp::resolver tcpResolver;
	asio::streambuf readBuffer; 
	asio::deadline_timer connectTimer;
	string host;
	int port;

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

	void readHandler(
		const boost::system::error_code& ec,
		std::size_t bytesTransferred);

	void writeHandler(
		shared_ptr<string> stringPtr,
		const boost::system::error_code& ec, 
		std::size_t bytesTransferred);

	void connectTimerHandler(
		const boost::system::error_code& ec
	);

	void doDisconnectHandler();

	void disconnected(const boost::system::error_code& ec);

};



#endif

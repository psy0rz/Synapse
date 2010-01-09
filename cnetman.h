//
// C++ Interface: cnet
//
// Description: Generic thread-safe networking class for synapse modules.
//              Creates and handles Cnet-sessions.
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CNETMAN_H
#define CNETMAN_H

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <string>

using namespace std;
using namespace boost;
using asio::ip::tcp;

typedef shared_ptr<tcp::acceptor> CacceptorPtr;
#include "cnet.h"

class CnetMan
{
	friend class Cnet;

	public:
	CnetMan ();
	virtual ~CnetMan();

	//for server: call runListen to listen on a port. 
	//It returns when doClose is called.
	//Add ids that will accept the new connections with runAccept. (runAccept returns when connection is closed)
	bool runListen(int port);
	bool doClose(int port);
	bool runAccept(int port, int id);

	//for client: call runConnect(returns when connection is closed)
	bool runConnect(int id, string host, int port, int reconnectTime=0);

	//for both client and server:
	bool doDisconnect(int id); 
	bool doWrite(int id, string & data);
	void doShutdown();

	private:
	typedef map<int, CnetPtr> CnetMap;
	CnetMap nets;

	typedef map<int, CacceptorPtr> CacceptorMap;
	CacceptorMap acceptors;

	mutex threadMutex;

	void closeHandler(int port);

	//callbacks for server
    virtual void listening(int port);
	virtual void accepting(int port, int id);
    virtual void closed(int port);
 
	//callbacks for client
    virtual void connecting(int id, string host, int port);

	//callbacks for client and server 
    virtual void connected(int id);
    virtual void disconnected(int id, const boost::system::error_code& error);
    virtual void read(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred);
	

};


#endif

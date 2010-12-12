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






// Threadsafe because we only post stuff with io_serivce::post()
// The CnetMan class is responsible for handling the creation of these Cnet-objects.








#ifndef CNET_H
#define CNET_H

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <string>
#include <boost/shared_ptr.hpp>
#include "cvar.h"

#define CONNECT_TIMEOUT 60

namespace synapse
{
	using namespace std;
	using namespace boost;
	using asio::ip::tcp;


	typedef shared_ptr<tcp::acceptor> CacceptorPtr;


	class Cnet
	{

		public:

		Cnet();
		virtual ~Cnet();

		//end-user api to ask us to DO stuff: (usually called from CnetMan)
		void doDisconnect();
		void doAccept(int id, CacceptorPtr acceptorPtr);
		void doConnect(int id, string host, int port, int reconnectTime=0, string delimiter="\n");
		void doWrite(string & data);
		void doWrite(shared_ptr<asio::streambuf> bufferPtr);
		void run();

		//admin/debugging
		virtual void getStatus(Cvar & var);

		//this needs to be public, in case someone overrides startAsyncRead():
		void readHandler(
			const boost::system::error_code& ec,
			std::size_t bytesTransferred);

		asio::io_service ioService;

		private:
		int reconnectTime;

		tcp::resolver tcpResolver;
		asio::deadline_timer connectTimer;


		string host; //the host to connect to, in client mode.
		int port; //the port to connect to in client mode

		//just stuff for getStatusStr, not neccesary for anything else.
		int statusServer;

		protected:

		//change these settings in the init() callback handler.
		string delimiter;

		//this needs to be protected, in case someone overrides startAsyncRead():
		asio::streambuf readBuffer;
		tcp::socket tcpSocket;

		//never change this, CnetMan assigns it to you
		int id;


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

	
		void writeStringHandler(
			shared_ptr<string> stringPtr,
			const boost::system::error_code& ec,
			std::size_t bytesTransferred);
	
		void writeStreambufHandler(
			shared_ptr<asio::streambuf> bufferPtr,
			const boost::system::error_code& ec,
			std::size_t bytesTransferred);
	
		void connectTimerHandler(
			const boost::system::error_code& ec
		);

		void disconnectHandler();

		void reset(const boost::system::error_code& ec);

		//you might want to override this if you need more flexible reading.
		//(usefull for http servers etc)
		virtual void startAsyncRead();

		//end-user "hooks" for server
		virtual void init_server(int id, CacceptorPtr acceptorPtr);
		virtual void accepting(int id, int port);
		virtual void connected_server(int id, const string &host, int port, int local_port);

		//end-user "hooks" for client
		virtual void connecting(int id, const string &host, int port);
		virtual void connected_client(int id, const string &host, int port);

		//end-user "hooks" for client and server
		virtual void init(int id);
		virtual void connected(int id, const string &host, int port);
		virtual void disconnected(int id, const boost::system::error_code& error);
		virtual void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred);

	};
}


#endif

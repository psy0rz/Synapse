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





//serial port handling with boost::asio
// Threadsafe because we only post stuff with io_serivce::post()



#ifndef CSERIAL_H
#define CSERIAL_H

//#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <string>
#include <boost/shared_ptr.hpp>
#include "cvar.h"


namespace synapse
{
	using namespace std;
	using namespace boost;



	class Cnet
	{

		public:

		Cserial();
		virtual ~Cserial();

		//end-user api to ask us to DO stuff: 
		void doOpen(int id, string delimiter, string port, 
			boost::asio::serial_port_base::baud_rate baud_value,
			boost::asio::serial_port_base::flow_control flow_control_value,
			boost::asio::serial_port_base::parity parity_value,
			boost::asio::serial_port_base::stop_bits stop_bits_value,
			boost::asio::serial_port_base::character_size character_size
		);
		void doClose();
		void doWrite(string & data);
		void doWrite(shared_ptr<asio::streambuf> bufferPtr);
		void run();

		//admin/debugging
		// virtual void getStatus(Cvar & var);

		//this needs to be public, in case someone overrides startAsyncRead():
		void readHandler(
			const boost::system::error_code& ec,
			std::size_t bytesTransferred);

		asio::io_service ioService;

		private:


		protected:

		//change these settings in the init() callback handler.
		string delimiter;

		//this needs to be protected, in case someone overrides startAsyncRead():
		asio::streambuf readBuffer;
		serial_port serialPort;

		//never change this, CnetMan assigns it to you
		int id;


		private:

		//Internal functions


		void writeStringHandler(
			shared_ptr<string> stringPtr,
			const boost::system::error_code& ec,
			std::size_t bytesTransferred);
	
		void writeStreambufHandler(
			shared_ptr<asio::streambuf> bufferPtr,
			const boost::system::error_code& ec,
			std::size_t bytesTransferred);
	

		void reset(const boost::system::error_code& ec);

		//you might want to override this if you need more flexible reading.
		//(usefull for http servers etc)
		virtual void startAsyncRead();

		// //end-user "hooks" for server
		// virtual void init_server(int id, CacceptorPtr acceptorPtr);
		// virtual void accepting(int id, int port);
		// virtual void connected_server(int id, const string &host, int port, int local_port);

		// //end-user "hooks" for client
		// virtual void connecting(int id, const string &host, int port);
		// virtual void connected_client(int id, const string &host, int port);

		//end-user "hooks" for client and server
		// virtual void init(int id);
		// virtual void connected(int id, const string &host, int port);
		// virtual void disconnected(int id, const boost::system::error_code& error);
		virtual void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred);

	};
}


#endif

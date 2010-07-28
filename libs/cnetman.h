// Description: Generic thread-safe networking class for synapse modules.
//              Creates and handles Cnet-sessions.


/** Synapse network states/flow:
	
	CnetMan is threadsafe, Cnet is NOT.
	
	id is optional: when 0 it will assign an id automaticly.

	For clients:
		runConnect(id,host,port,reconnectTime)
			-> returns false if error 
			-> calls back connecting(id)
			on succesfull resolving and connecting:
				-> calls back connected(id)
				-> calls back connected_client(id);
				-> calls back read(id,...) for all incoming data until disconnected
			-> calls back disconnected(id,error); 
			if reconnectTime>0 and doDisconnect was not called: sleeps and starts over again..
			-> returns true;

	For servers:
		First call runListen to open the port:
		runListen(int port)
			-> returns false if error 
			-> listens on port, keeps running until user calls doClose(port) or until error
			-> returns true
							
		Now call runAccept to accept new connections:
		runAccept(int port, int id)
			-> returns false if errror
			-> calls back accepting(port,id)
			-> waits for new connection on port until user calls doDisconnect(id).or until error
			on newly accepted connection:
				-> calls back connected(id);
				-> calls back connected_server(id);
				-> calls back received(id,...) for all incoming data until disconnected
			-> calls back disconnected(id,error); 
			-> returns true;

	Writing data can be done with doWrite(..). Offcourse only after connected(id) was called back.
		if writing fails a disconnection occurs.

	Server and client code can be almost the same so its very easy to change your client into a server or vice versa: Only the first few steps are different, after the connection is eshtablised all the calls and callbacks are the same.


*/


#ifndef CNETMAN_H
#define CNETMAN_H

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <string>


typedef boost::shared_ptr<boost::asio::ip::tcp::acceptor> CacceptorPtr;
#include "cnet.h"

namespace synapse
{


	using namespace std;
	using namespace boost;
	using asio::ip::tcp;

	template <class Tnet>
	class CnetMan
	{

		public:
		CnetMan(unsigned int maxConnections);

		//for server: call runListen to listen on a port.
		//It returns when doClose is called.
		//Add ids that will accept the new connections with runAccept. (runAccept returns when connection is closed)
		bool runListen(int port);
		bool doClose(int port);
		bool runAccept(int port, int id);

		//for client: call runConnect(returns when connection is closed)
		bool runConnect(int id, string host, int port, int reconnectTime=0, string delimiter="\n");

		//for both client and server:
		bool doDisconnect(int id);
		bool doWrite(int id, string & data);
		void doShutdown();
		void setMaxConnections(unsigned int maxConnections);

		//debugging/admin
		string getStatusStr();

		typedef shared_ptr<Tnet> CnetPtr;
		typedef map<int, CnetPtr > CnetMap;
		typedef map<int, CacceptorPtr> CacceptorMap;

		//also publicly avaible for easy access to all your connections
		//be carefull with locking!
		CnetMap nets;
		mutex threadMutex;
		condition_variable threadCond;


		private:
		bool shutdown;
		int autoIdCount;
		CacceptorMap acceptors;

		unsigned int maxConnections;

		void closeHandler(int port);
		int getAutoId();

	
	};
}

/// Templates need to be compiled for every use-case:
#include "cnetman.cpp"

#endif

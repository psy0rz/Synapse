
#include "clog.h"
#include "cnet.h"

/// NOTE: since this is a template, this cpp file will be included from cnetman.h! (and thus re-compiled for every module)

//using namespace std;
//using namespace boost;
//using asio::ip::tcp;

namespace synapse
{

using namespace std;
using namespace boost;
using asio::ip::tcp;


template <class Tnet> 
CnetMan<Tnet>::CnetMan(unsigned int maxConnections=100)
{
	shutdown=false;
	autoIdCount=0;	
	this->maxConnections=maxConnections;
}

template <class Tnet> 
int CnetMan<Tnet>::getAutoId()
{	
	//NOTE: we assume we already are locked!
	autoIdCount++;
	while (autoIdCount==0 || nets.find(autoIdCount)!=nets.end())
		autoIdCount++;

	return autoIdCount;
}

template <class Tnet> 
bool CnetMan<Tnet>::runConnect(int id, string host, int port, int reconnectTime, string delimiter)
{
	CnetPtr netPtr;
	{
		lock_guard<mutex> lock(threadMutex);

		if (nets.size()>=maxConnections)
		{
			ERROR("net reached max connections of " << maxConnections << ", cannot create new connection.");
			return false;
		}

		if (id==0)
		{
			id=getAutoId();
		}
		else if (nets.find(id)!=nets.end())
		{
			ERROR("net id " << id << " is already busy, ignoring connect-request");
			return false;
		}

		//add a new object to the list and do the connect
		netPtr=(CnetPtr(new Tnet()));
		nets[id]=netPtr;
		netPtr->doConnect(id,host,port,reconnectTime,delimiter);
	}

	//let the ioservice run, until the connection is closed again:
	netPtr->run();
	DEB("ioservice finished for id " << id);

	{
		lock_guard<mutex> lock(threadMutex);
		//we're done, remove the Cnetobject from the list
 		nets.erase(id);
	}

	return true;
}

template <class Tnet> 
bool CnetMan<Tnet>::runListen(int port)
{
	asio::io_service ioService;
	{
		lock_guard<mutex> lock(threadMutex);
		if (acceptors.find(port)!=acceptors.end())
		{
			ERROR("net port " << port << " is already listening, ignoring listen-request");
			return false;
		}

		//listen on the port and add to the list
		acceptors[port]=CacceptorPtr(new tcp::acceptor(ioService,tcp::endpoint(tcp::v4(), port)));

		//inform the world we're listening
		INFO("Listening on tcp port " << port);

		//inform any runAccepts that are waiting for us
		threadCond.notify_all();

	}
	//at this point the ioservice has no work yet, so make sure it keeps running:
	asio::io_service::work work(ioService);

	//let the ioservice run, until the acceptor is closed 
	ioService.run();
	{
		lock_guard<mutex> lock(threadMutex);
		//we're done, remove the acceptor from the list
		acceptors.erase(port);
		//FIXME: what happens if a runAccept JUST started a threadCond.wait()? will it hang until shutdown or the next runListen?
	}
	INFO("Stopped listening on tcp port " << port);

	return true;
}

template <class Tnet> 
bool CnetMan<Tnet>::runAccept(int port, int id)
{
	CnetPtr netPtr;
	{
		unique_lock<mutex> lock(threadMutex);

		if (nets.size()>=maxConnections)
		{
			ERROR("net reached max connections of " << maxConnections << ", cannot accept new connection.");
			return false;
		}

		while (!shutdown && acceptors.find(port)==acceptors.end())
		{
			DEB("net port " << port << " is not listening yet, waiting...");
			threadCond.wait(lock);
		}

		if (id==0)
		{
			id=getAutoId();
		}
		else if (nets.find(id)!=nets.end())
		{
			ERROR("net id " << id << " is already busy, ignoring accept-request on port " << port);
			return false;
		}
		//create a new net-object that will async_accept the next connection from acceptors[port]
		netPtr=(CnetPtr(new Tnet()));
		nets[id]=netPtr;
		netPtr->doAccept(id,acceptors[port]);
	}

	//let the ioservice run, until the connection is closed again:
	netPtr->run();
	DEB("ioservice finished for port " << port << " into " << id);

	{
		lock_guard<mutex> lock(threadMutex);
		//we're done, remove the Cnetobject from the list
 		nets.erase(id);
	}

	return true;
}

template <class Tnet> 
bool CnetMan<Tnet>::doClose(int port)
{
	{
		lock_guard<mutex> lock(threadMutex);
		if (acceptors.find(port)==acceptors.end())
		{
			ERROR("net port " << port << " is not listening, ignoring close-request");
			return false;
		}

		//post via ioservice because acceptor is not threadsafe
		DEB("Posting close request for port " << port);
		acceptors[port]->get_io_service().post(bind(&CnetMan::closeHandler,this,port));
	}
	return true;
}

//called from the doListen->ioService->run() thread:
template <class Tnet> 
void CnetMan<Tnet>::closeHandler(int port)
{
	acceptors[port]->close();
	acceptors[port]->get_io_service().stop();
	//after cancelling all pending accepts, doListen will now return
}

template <class Tnet> 
bool CnetMan<Tnet>::doDisconnect(int id)
{
	{
		lock_guard<mutex> lock(threadMutex);
		if (nets.find(id)==nets.end())
		{
			DEB("id " << id << " does not exist, ignoring disconnect request");
			return false;
		}
		nets[id]->doDisconnect();
	}
	return true;
}

template <class Tnet> 
bool CnetMan<Tnet>::doWrite(int id, string & data)
{
	{
		lock_guard<mutex> lock(threadMutex);
		if (nets.find(id)==nets.end())
		{
			ERROR("net id " << id << " does not exist, ignoring write request");
			return false;
		}
		nets[id]->doWrite(data);
	}
	return true;
}


template <class Tnet> 
void CnetMan<Tnet>::doShutdown()
{
	{
		lock_guard<mutex> lock(threadMutex);

		if (shutdown)
			return;

		shutdown=true;

		//close all ports
		DEB("Closing all open ports");	
		for (CacceptorMap::iterator acceptorI=acceptors.begin(); acceptorI!=acceptors.end(); acceptorI++)
		{
			acceptorI->second->get_io_service().post(bind(&CnetMan::closeHandler,this,acceptorI->first));
		}

		//FIXME: we need a sleep here? since it takes a while to post and process the request, and in that time new connections could arrive?

		//disconnect all connections
		DEB("Disconneting all connections");	
		for (typename CnetMap::iterator netI=nets.begin(); netI!=nets.end(); netI++)
		{
			netI->second->doDisconnect();
		}


	}
}

template <class Tnet> 
void CnetMan<Tnet>::setMaxConnections(unsigned int maxConnections)
{
	lock_guard<mutex> lock(threadMutex);
	this->maxConnections=maxConnections;
}

template <class Tnet>
string CnetMan<Tnet>::getStatusStr()
{
	{
		lock_guard<mutex> lock(threadMutex);
		stringstream s;

		for (CacceptorMap::iterator acceptorI=acceptors.begin(); acceptorI!=acceptors.end(); acceptorI++)
		{
			s << "Listening on: " << acceptorI->first << "/tcp\n";
		}

		s << "Open connections: \n";
		for (typename CnetMap::iterator netI=nets.begin(); netI!=nets.end(); netI++)
		{
			string status;
			status=netI->second->getStatusStr();
			if (status!="")
			{	
				s << status << "\n";
			}
		}

		return (s.str());
	}

};

}

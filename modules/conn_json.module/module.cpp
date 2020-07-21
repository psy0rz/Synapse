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


#include "cnetman.h"
#include "synapse.h"
#include <boost/thread/condition.hpp>

//TODO: implement queueing, so we can handle disconnect+reconnects (with a fallback option if a message is missed: this will just end all the sessions like currently happens)


#define MAX_MESSAGE 2048

using namespace boost;
using namespace std;

int moduleSessionId=0;

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	moduleSessionId=msg.dst;

	//change module settings.
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=100;
	out["broadcastCookie"]=1;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=100;
	out.send();

	//register a special handler without specified event
	//this will receive all events that are not handled elsewhere in this module.
	out.clear();
	out.event="core_Register";
	out["handler"]="all";
	out.send();

	//tell the rest of the world we are ready for duty
	//Cmsg out;
	out.event="core_Ready";
	out.send();

}



// We extent the Cnet class with our own network handlers.

// As soon as something with a network connection 'happens', these handlers will be called.
// This stuff basically runs as anonymous, until a user uses core_login to change the user.
class CnetModule : public synapse::Cnet
{
	public:
	typedef map<int,int> CsessionMapping;
	//both lists are identical but reversed, just for peformance reasons.
	CsessionMapping sessionRemoteToLocalSrc;
	CsessionMapping sessionLocalToRemoteDst;
	string username;
	string password;


	public:
	void sendMessage(Cmsg & msg)
	{
		//if its not a broadcast, map it
		if (msg.dst)
		{
			//does a mapping exist?
			CnetModule::CsessionMapping::iterator sessionMappingI;
			sessionMappingI=sessionLocalToRemoteDst.find(msg.dst);
			if (sessionMappingI!=sessionLocalToRemoteDst.end())
			{
				//yes, so map it
				msg.dst=sessionMappingI->second;
			}
			else
			{
				ERROR("conn_json: trying to send message to network connection " << id << " and msg.dst " << msg.dst << ", but i can not find a session-mapping for this destination.");
				return;
			}
		}

		//now send it over the network connection:
		string msgStr;
		msg.toJson(msgStr);
		msgStr+="\n";
		//TODO: we can optimize this with a direct syncronious write?
		doWrite(msgStr);
	}


	private:

	/** Server connection 'id' is established.
	*/
 	void connected_server(int id, const string &host, int port, int local_port)
	{
		Cmsg out;

		//FIXME: TEMPORARY HACK
		username="psy";
		password="as";


		//fireoff a new acceptor thread, to accept future connections.
		out.clear();
		out.dst=moduleSessionId;
		out.event="conn_json_Accept";
		out["port"]=local_port;
 		out.send();


 		//someone has connecting us, create a new session initial session for this connection
//		out.clear();
//		out.dst=0;
//		out.event="core_NewSession";
//		out["synapse_cookie"]=id;
//		out["username"]="anonymous";
//		out["password"]="anonymous";
//		out.send();

	}


	/** Connection 'id' has received new data.
	*/
	void received(int id, asio::streambuf &readBuffer, std::size_t bytesTransferred)
	{
		Cmsg out;
		string dataStr(boost::asio::buffer_cast<const char*>(readBuffer.data()), readBuffer.size());
		dataStr.resize(dataStr.find(delimiter)+delimiter.length());

		if (dataStr.length() > MAX_MESSAGE)
		{
			stringstream s;
			s << "Json message on id " << id << " is " << bytesTransferred << " bytes. (max=" << MAX_MESSAGE << ") Ignoring.";
			Cmsg errMsg;
			errMsg["description"]=s.str();
			errMsg.event="error";
			sendMessage(errMsg);
		}
		else
		{
			try
			{
				//TODO: replace 'error'-string with exceptions?
				string error;

				//parse it
				out.fromJson(dataStr);

				//map the remote session to a local session:
				//do we already have a mapping for this one?
				CsessionMapping::iterator sessionMappingI;
				sessionMappingI=sessionRemoteToLocalSrc.find(out.src);
				if (sessionMappingI!=sessionRemoteToLocalSrc.end())
				{
					//found it, so just map it
					out.src=sessionMappingI->second;
				}
				else
				{
					//no mapping yet: create a new session mapping on the fly, by approaching the core directly!
					//(normally this would be considered a hack, but for the special case of connector-modules its ok i think)
					boost::lock_guard<boost::mutex> lock(synapse::messageMan->threadMutex);
					synapse::CsessionPtr session=synapse::messageMan->userMan.getSession(moduleSessionId);
					if (!session)
						error="BUG: conn_json cant find module session.";
					else
					{
						//create the session, and use the netId as cookie.
						synapse::CsessionPtr newSession=synapse::CsessionPtr(new synapse::Csession(session->user,session->module,id));
						int sessionId=synapse::messageMan->userMan.addSession(newSession);
						if (sessionId==SESSION_DISABLED)
							error="cant create new session";
						else
						{
							//login with the supplied username/password
							error=synapse::messageMan->userMan.login(sessionId, username, password);

							if (error!="")
							{
								//login failed, delete session again
								synapse::messageMan->userMan.delSession(sessionId);
							}
							else
							{
								//new session + login succeeded, add to our mapping tables
								sessionRemoteToLocalSrc[out.src]=sessionId;
								sessionLocalToRemoteDst[sessionId]=out.src;

								stringstream s;
								s << "Session mapping from " << out.src << "@" << tcpSocket.remote_endpoint().address() << ":" << tcpSocket.remote_endpoint().port();
								newSession->description=s.str();

								//now actually map it:
								out.src=sessionId;
							}
						}
					}
				}

				//if nothing went wrong, send the session-mapped message:
				if (error=="")
				{
					out.send(id);
				}
				else
				//something went wrong?
				{
					Cmsg errMsg;
					errMsg["description"]=error;
					errMsg.event="error";
					sendMessage(errMsg);
				}
			}
			catch(std::exception& e)
			{
				Cmsg errMsg;
				errMsg["description"]=string(e.what());
				errMsg.event="error";
				sendMessage(errMsg);
			}
		}
		//cant we do this right away, before parsing ?
		readBuffer.consume(dataStr.length());

	}


	/** Connection 'id' is disconnected, or a connect-attempt has failed.
	* Sends: conn_Disconnected
	*/
 	void disconnected(int id, const boost::system::error_code& ec)
	{
		Cmsg out;
		out.event="core_DelCookieSessions";
		out["cookie"]=id;
		out.send();
	}
};


synapse::CnetMan<CnetModule> net;

//void sendToNetId(int id, Cmsg & msg)
//{
//	else if (!id && msg.dst==0)
//	{
//		//no specific id, and its a broadcast?
//		//send it to all net objects:
//		boost::lock_guard<boost::mutex> lock(net.threadMutex);
//
//		for (synapse::CnetMan<CnetModule>::CnetMap::iterator netI=net.nets.begin(); netI!=net.nets.end(); netI++)
//		{
//			netI->second->ioService.post(bind(&CnetModule::sendMessage,netI->second, msg));
//		}
//	}
//}




/** Server only: Creates a new server and listens specified port
 */
SYNAPSE_REGISTER(conn_json_Listen)
{
	if (msg.dst==moduleSessionId)
	{
		//starts a new thread to accept and handle the incomming connection.
		Cmsg out;

		//fire off acceptor thread
		out.clear();
		out.dst=msg.dst;
		out.event="conn_json_Accept";
		out["port"]=msg["port"];
 		out.send();

		//become the listening thread
		net.runListen(msg["port"]);

	}
	else
		ERROR("Send to the wrong session id");
}


/** Server only: Stop listening on a port
 */
SYNAPSE_REGISTER(conn_json_Close)
{
	net.doClose(msg["port"]);
}

/** Runs an acceptor thread (only used internally)
 */
SYNAPSE_REGISTER(conn_json_Accept)
{
	if (msg.dst==moduleSessionId)
	{
		net.runAccept(msg["port"], 0);
	}

}



/** Called when synapse whats the module to shutdown completely
 * This makes sure that all ports and network connections are closed, so there wont be any 'hanging' threads left.
 * If you care about data-ordering, send this to session-id that sended you the net_Connected.
 */
SYNAPSE_REGISTER(module_Shutdown)
{
	//let the net module shut down to fix the rest
	net.doShutdown();
}



/** This handler is called for all events that:
 * -dont have a specific handler,
 * -are send to broadcast or to a session we manage.
 */
SYNAPSE_HANDLER(all)
{
	//if its for a specific network connection, send it to the correct net-object for furhter processing.
	if (cookie)
	{
		//accessing the net-object directly, so lock it:
		boost::lock_guard<boost::mutex> lock(net.threadMutex);

		synapse::CnetMan<CnetModule>::CnetMap::iterator netI;
		netI=net.nets.find(cookie);
		if (netI!=net.nets.end())
		{
			//found the net object, pass the message on the the netobject and let it decide that to do with it.
			//We need to post it, since the net-object gets called by the ioservice thread, so there is no other safe way to call it:
			netI->second->ioService.post(bind(&CnetModule::sendMessage,netI->second, msg));
		}
		else
		{
			ERROR("conn_json: trying to send message to non-existant network connection with id " << cookie);
			return ;
		}
	}
}

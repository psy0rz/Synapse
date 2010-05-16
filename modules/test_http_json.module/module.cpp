#include "synapse.h"
#include <map>
#include <string>
#include <boost/regex.hpp>

using namespace std;
using namespace boost;

mutex threadMutex;


SYNAPSE_REGISTER(module_Init)
{

	INFO("Regression test for: http_json.");
	Cmsg out;

	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=100;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=100;
	out.send();

	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"test_http_json_Randomdata"; 
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"anonymous";
	out.send();

// 	out.event="test_http_json_Randomdata";
// 	out.send();

	out.clear();
	out.event="core_LoadModule";
	out.dst=1;
	out["path"]="modules/net.module/libnet.so";
	out.send();

	out.clear();
	out.event="core_LoadModule";
	out.dst=1;
	out["path"]="modules/http_json.module/libhttp_json.so";
	out.send();

// 	out.clear();
// 	out.event="core_ChangeLogging";
// 	out["logSends"]=0; 
// 	out["logReceives"]=0;
// 	out.send();

}

//90 of the 100 so we can still monitor with our browser
#define NET_SESSIONS 90

SYNAPSE_REGISTER(http_json_Ready)
{
	Cmsg out;
	out.clear();
	out.event="http_json_Listen";
	out["port"]="10080";
	out.dst=msg["session"];
	out.send();
}

// SYNAPSE_REGISTER(test_http_json_Randomdata)
// {
// 	sleep(1);
// 	Cmsg out;
// 	out.event="test_http_json_Randomdata";
// 	out["random1"].str().resize(random()%40, '1');
// 	out["random2"].str().resize(random()%40, '2');
// 	out["sub1"]["random3"].str().resize(random()%40, '3');
// 	if (random()%2 )
// 		out["random4"].str().resize(random()%40, '4');
// 	out.send();
// 
// }


SYNAPSE_REGISTER(net_Ready)
{
	Cmsg out;

	//wait for other stuff to get ready too
	sleep(3);

	//create sessoins for connections
	for (int a=0; a<NET_SESSIONS; a++)
	{
		out.clear();
		out.event="core_NewSession";
		out.send();
	};

}

SYNAPSE_REGISTER(module_SessionStart)
{
	Cmsg out;
	out.clear();
	out.event="net_Connect";
	out.src=msg.dst;
	out["port"]=10080;
	out["host"]="localhost";
	out["delimiter"]="\r\n\r\n";
	out.send();
}


SYNAPSE_REGISTER(net_Connected)
{
	DEB("test_http_json:  CONNECTED " << msg.dst << " sending first request");
	Cmsg out;
	out.src=msg.dst;
	out.dst=msg.src;
	out["data"]="GET /synapse/longpoll HTTP/1.1\r\n\r\n";
	out.event="net_Write";
	out.send();
}


SYNAPSE_REGISTER(net_Read)
{
//	INFO("test_http_json: GOT data for " << msg.dst << ": " << msg["data"].str());

	//received data?
	if (msg.isSet("data"))
	{
		//try to parse authcookie
		smatch what;
		if (regex_search(
			msg["data"].str(),
			what, 
			boost::regex("X-Synapse-Authcookie: (.*)")
		))
		{
			string authCookie=what[1];
			DEB("test_http_json: Got authcookie: " << authCookie);

			Cmsg out;

			//send random broadcast message request
			string jsonStr="[0,0,\"test_http_json_Randomdata\",{";

			string rnd;
			rnd.resize(random()%40, '1');
			jsonStr+="\"random1\":\""+rnd+"\"";

			rnd="";
			rnd.resize(random()%40, '2');
			jsonStr+=",\"random2\":\""+rnd+"\"";

			rnd="";
			rnd.resize(random()%40, '3');
			jsonStr+=",\"random3\":\""+rnd+"\"";

			jsonStr+="}";

			stringstream ss;
			ss << "POST /synapse/send HTTP/1.1\r\nX-Synapse-Authcookie: "+authCookie+"\r\n";
			ss << "content-length: " << jsonStr.length() << "\r\n";
			ss << "\r\n";
			ss << jsonStr;

			out.clear();
			out.src=msg.dst;
			out.dst=msg.src;
			out.event="net_Write";
			out["data"]=ss.str();
			out.send();

			//sleep to test random session timeouts
			if (random()%100 == 0)
			{	
				//timeout should be 10 seconds..
				DEB("Letting session timeout ");
				sleep(20);
			}
	
			//send longpollrequest
			//Now we will offcourse receive back our own broadcast.
			out.clear();
			out.src=msg.dst;
			out.dst=msg.src;
			out.event="net_Write";
			out["data"]="GET /synapse/longpoll HTTP/1.1\r\nX-Synapse-Authcookie: "+authCookie+"\r\n\r\n";
			out.send();

			//random disconnect after a while
			if (random()%100 == 0)
			{	
				DEB("Random disconnect ");
				Cmsg out;
				out.clear();
				out.event="net_Disconnect";
				out.src=msg.dst;
				out.dst=msg.src;
				out.send();
			}

		}
/*		//no cookie found or error retruned, request longpoll again
		else
		{
			Cmsg out;
			out.src=msg.dst;
			out.dst=msg.src;
			out["data"]="GET /synapse/longpoll HTTP/1.1\r\n\r\n";
			out.event="net_Write";
			out.send();
		}*/
	}

	
}

SYNAPSE_REGISTER(net_ConnectEnded)
{
	Cmsg out;
	out.clear();
	out.event="net_Connect";
	out.src=msg.dst;
	out["port"]=10080;
	out["host"]="localhost";
	out["delimiter"]="\r\n\r\n";
	out.send();

}




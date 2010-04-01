#include "synapse.h"

int counter;
int counterSleep;

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=10;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=10;
	out.send();

	///tell the rest of the world we are ready for duty
	out.clear();
	out.event="core_Ready";
	out.send();

	
	//load the modules you wanna fiddle around with:
	out.clear();
	out.event="core_LoadModule";

// 	out["path"]="modules/lirc.module/liblirc.so";
// 	out.send();

//	out["path"]="modules/net.module/libnet.so";
//	out.send();

// 	out["path"]="modules/conn_json.module/libconn_json.so";
// 	out.send();

	out["path"]="modules/http_json.module/libhttp_json.so";
	out.send();

//  	out["path"]="modules/ami.module/libami.so";
//  	out.send();

 	out["path"]="modules/asterisk.module/libasterisk.so";
 	out.send();

	// Counter that ever counterSleep seconds emits a message.
	// Speed can be changed with appropriate messages
	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"test_Counter"; 
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"modules";
	out["recvGroup"]=	"anonymous";
	out.send();

	counter=0;
	counterSleep=10000000;
	out.clear();
	out.event="test_Counter";
	out.send();


	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"chat_Text"; 
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"anonymous";
	out.send();

	out.clear();
	out.event="core_ChangeEvent";
	out["event"]=		"chat_Poll"; 
	out["modifyGroup"]=	"modules";
	out["sendGroup"]=	"anonymous";
	out["recvGroup"]=	"anonymous";
	out.send();

}

SYNAPSE_REGISTER(asterisk_Ready)
{
	Cmsg out;

// 
	out.clear();
	out.event="asterisk_Connect";
	out["username"]="manager";
	out["password"]="insecure";
	out["host"]="localhost";
	out["port"]="5038";
	out.send();
// 
// 	out.clear();
// 	out.event="asterisk_Connect";
// 	out["username"]="manager";
// 	out["password"]="insecure";
// 	out["host"]="localhost";
// 	out["port"]="5039";
// 	out.send();
// 
// 	out.clear();
// 	out.event="asterisk_Connect";
// 	out["username"]="manager";
// 	out["password"]="insecure";
// 	out["host"]="localhost";
// 	out["port"]="5040";
// 	out.send();


	//replay server with:
	//tcpserver 0.0.0.0 5555 scriptreplay mt2.timing mt2.txt

	out.clear();
	out.event="asterisk_Connect";
	out["username"]="bla";
	out["password"]="bla";
	out["host"]="localhost";
	out["port"]="5555";
	out.send();


	out.clear();
	out.event="asterisk_Connect";
	out["username"]="admin";
	out["password"]="s3xl1jn";
	out["host"]="192.168.13.1";
	out["port"]="5038";
	out.send();

}

SYNAPSE_REGISTER(test_Counter)
{
	usleep(counterSleep);
	counter++;
	Cmsg out;
	out.event="test_Counter";
	out["counter"]=counter;
	out["counterSleep"]=counterSleep;
	out.send();

}

void counterStatus()
{
	if (counterSleep<10000)
		counterSleep=10000;

	if (counterSleep>5000000)
		counterSleep=5000000;


	Cmsg out;
	out.event="test_CounterStatus";
	out["counterSleep"]=counterSleep;
	out.send();
}


SYNAPSE_REGISTER(test_CounterFaster)
{
	counterSleep=counterSleep-100000;
	counterStatus();
}

SYNAPSE_REGISTER(test_CounterSlower)
{
	counterSleep=counterSleep+100000;
	counterStatus();
}

SYNAPSE_REGISTER(test_CounterSpeed)
{
	counterSleep=msg["counterSleep"];
	counterStatus();
}



SYNAPSE_REGISTER(http_json_Ready)
{
	Cmsg out;
	out.clear();
	out.event="http_json_Listen";
	out["port"]="10080";
	out.dst=msg["session"];
	out.send();
}



SYNAPSE_REGISTER(loop)
{

	Cmsg out;
	out.clear();
	out.event="loop";
	out["bla"]="loopback test";
	out.dst=msg.dst;
	out.send();
}

 
SYNAPSE_REGISTER(lirc_Ready)
{
	Cmsg out;
	out.clear();
	out.event="lirc_Connect";
	out["host"]="192.168.13.1";
	out["port"]="8765";
	out.send();


}


SYNAPSE_REGISTER(conn_json_Ready)
{
	Cmsg out;
	out.clear();
	out.event="conn_json_Listen";
	out["port"]="12345";
	out.dst=msg["session"];
	out.send();
}

SYNAPSE_REGISTER(net_Ready)
{
	Cmsg out;
	out.clear();
	out.event="net_Connect";
	out["host"]="192.168.13.1";
	out["port"]="8765";
	out.send();


}

SYNAPSE_REGISTER(net_Read)
{
}
SYNAPSE_REGISTER(lirc_Read)
{
}

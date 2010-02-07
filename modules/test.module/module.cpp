#include "cnet.h"
#include "synapse.h"

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

	out["path"]="modules/lirc.module/liblirc.so";
	out.send();

//	out["path"]="modules/net.module/libnet.so";
//	out.send();

	out["path"]="modules/conn_json.module/libconn_json.so";
	out.send();


// 	sleep(5);
// 	out.event="core_Shutdown";
// 	out.dst=0;
// 	out.send();

// 	out.clear();
// 	out.event="core_ChangeLogging";
// 	out["logSends"]=0;
// 	out["logReceives"]=0;
// 	out.send();

// 	out.clear();
// 	out.event="loop";
// 	out["bla"]="loopback test";
// 	out.dst=msg.dst;
// 	out.send();


/*	for (int bla=0; bla < 10; bla++)
	{	
		out.clear();
		out.event="test";
		out.dst=0;
		out.clear();
		out.send();
		sleep(1);
	}

	out.event="core_Shutdown";
	out.dst=0;
	out.send();
*/

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
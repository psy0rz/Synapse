/** \file
The timer module.

This is a simple timer module, to send delayed events or repeat events with a interval timer.

*/


#include "synapse.h"
#include <time.h>

//FIXME: somehow the global shutdown variable clashes with a shutdown variable of another module. 
//Figure out whats going on, or define namespace automaticly?

namespace timer
{

bool shutdown;

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	shutdown=false;

	// max 10 threads / timers active
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=10;
	out.send();

	//tell the rest of the world we are ready for duty
	//(the core will send a timer_Ready)
	out.clear();
	out.event="core_Ready";
	out.send();
}

/** Sets a timer
	\param time Sets the timer to trigger at some absolute time in the future. (time is a unix timestmap)
	\param seconds Set the timer to trigger after \c seconds seconds.
	\param repeat Specifies how many times the timer should be repeated. -1 is infinite.
	\param event Specifies the event that should be send. All other parameters will be echoed back.
	\param dst Destination to send the event to.

\par Replies \c event:
	Sends the \c event every \c seconds seconds, for a \c repeat number of times. 
\par	
	OR: Sends the \c event at \c time.

\note There is no way to stop a infinite repeating timer. (we'll fix this with the Interrupt thread methods later)
*/
SYNAPSE_REGISTER(timer_Set)
{
	//absolute endtime given:
	if (msg.isSet("time"))
	{
		struct timespec now;
		if (clock_gettime(CLOCK_REALTIME,&now))
		{
			msg.returnError("Cant get time");
			return;
		}

		if ((msg["time"]-now.tv_sec)>600)
		{
			msg.returnError("Time too far in the future.");
			return;
		}
		struct timespec abstime;
		abstime.tv_sec=((int)msg["time"]);
		abstime.tv_nsec=(long int)((long double)msg["time"] - (int)msg["time"]  )*1000000000;
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &abstime ,NULL);

		Cmsg out(msg);
		out.src=0;
		out.dst=msg["dst"];
		out.event=(string)msg["event"];
		out.erase("event");
		out.send();
	}
	//relative time delta given:
	else if (msg.isSet("seconds"))
	{
		if (msg["seconds"]>600)
		{
			msg.returnError("Time delay too long.");
			return;
		}

		Cmsg out(msg);
		out.src=0;
		out.dst=msg["dst"];
		out.event=(string)msg["event"];
		out.erase("event");

		int repeat=1;
		if (msg.isSet("repeat"))
			repeat=msg["repeat"];

		int count=0;

		while ((!shutdown) && (count != repeat))
		{
			struct timespec reltime;
			reltime.tv_sec=((int)msg["seconds"]);
			reltime.tv_nsec=(long int)((long double)msg["seconds"] - (int)msg["seconds"]  )*1000000000;
			clock_nanosleep(CLOCK_REALTIME, 0, &reltime ,NULL);
			out.send();
			count++;
		}
	}
	else
	{
		msg.returnError("No time specified.");
		return;
	}

}


SYNAPSE_REGISTER(module_Shutdown)
{
	shutdown=true;
}

}

#include "synapse.h"
#include <time.h>


SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;


	/// max 10 threads / timers active
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=10;
	out.send();

	///tell the rest of the world we are ready for duty
	//(the core will send a timer_Ready)
	out.clear();
	out.event="core_Ready";
	out.send();
}


SYNAPSE_REGISTER(timer_Set)
{
	//absolute endtime given:
	if (msg["time"])
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
		abstime.tv_nsec=((long double)msg["time"] - (int)msg["time"]  )*1000000000;
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &abstime ,NULL);
	}
	//relative time delta given:
	else if (msg["seconds"])
	{
			if (msg["seconds"]>600)
			{
				msg.returnError("Time delay too long.");
				return;
			}
			struct timespec reltime;
			reltime.tv_sec=((int)msg["seconds"]);
			reltime.tv_nsec=((long double)msg["seconds"] - (int)msg["seconds"]  )*1000000000;
			clock_nanosleep(CLOCK_REALTIME, 0, &reltime ,NULL);
	}
	else
	{
		msg.returnError("No time specified.");
		return;
	}

	Cmsg out(msg);
	out.src=0;
	out.dst=msg["dst"];
	out.event=(string)msg["event"];
	out.erase("event");
	out.send();
}



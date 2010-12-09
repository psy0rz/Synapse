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


/** \file
The execute module.

This module allows you to execute command, while retreiving back output data and exit codes.



*/


#include "synapse.h"
#include <time.h>

namespace exec
{
	using namespace std;


	bool shutdown;

	SYNAPSE_REGISTER(module_Init)
	{
		Cmsg out;

		shutdown=false;

		out.clear();
		out.event="core_ChangeModule";
		out["maxThreads"]=10;
		out.send();

		out.clear();
		out.event="core_ChangeSession";
		out["maxThreads"]=10;
		out.send();

		//tell the rest of the world we are ready for duty
		//(the core will send a timer_Ready)
		out.clear();
		out.event="core_Ready";
		out.send();
	}

	/** Starts a process
		\param cmd The command(s) to execute. This will be passed to a shell (probably bash)

	\par Replies exec_Started:
		To indicate the process is started.
		\param pid The process id

	*/
	SYNAPSE_REGISTER(exec_Start)
	{

	}


	SYNAPSE_REGISTER(module_Shutdown)
	{
		shutdown=true;
	}

}

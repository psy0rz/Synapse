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
#include <stdio.h>
#include "cconfig.h"
#include <string.h>
#include <errno.h>

#include "exception/cexception.h"

namespace exec
{
	using namespace std;


	bool shutdown;
	int queueId=-1;
	int defaultId=-1;

	SYNAPSE_REGISTER(module_Init)
	{
		Cmsg out;
		shutdown=false;
		defaultId=msg.dst;

		//load config file
		synapse::Cconfig config;
		config.load("etc/synapse/exec.conf");

		out.clear();
		out.event="core_ChangeModule";
		out["maxThreads"]=config["maxProcesses"];
		out.send();

		out.clear();
		out.event="core_ChangeSession";
		out["maxThreads"]=config["maxProcesses"];
		out.send();

		out.clear();
		out.event="core_NewSession";
		out["maxThreads"]=1;
		out["description"]="Serial execution queue";
		out.send();

	}

	SYNAPSE_REGISTER(module_SessionStart)
	{
		if (msg.dst!=defaultId)
		{
			queueId=msg.dst;

			//tell the rest of the world we are ready for duty
			Cmsg out;
			out.clear();
			out.event="core_Ready";
			out.send();
		}
	}



	//TODO: make this bi-directional, so you also can send data to processes. not needed for now, so we keep it simple
	/** Starts a process
		\param cmd The command(s) to execute. This will be passed to a shell (probably bash)
		\param id An user supplid id that is send back with every event, so you can differntiate between different processes.
		\param queue Set to put the command in the serial execution queue. (e.g. dont execute parallel with other processes in the queue)

	\par Replies exec_Started:
		To indicate the process is started.
		\param id	The user supplied id.

	\par Replies exec_Data:
		To indicate data is received. This is line based, so every line generates an event.
		NOTE: Only stdout is send, so use redirections if neccesary, otherwise you'll see the output on the console.
		\param id	The user supplied id.
		\param data The data (string)

	\par Replies exec_Error:
		To indicate the starting of the process went wrong. No more messages follow after this.
		\param id	The user supplied id.
		\param error The error text, supplied by the os.

	\par Replies exec_Ended:
		To indicate the process has ended. No more messages follow after this.
		\param id	  The user supplied id.
		\param exit   The program exited normally, with this code.
		\param signal The program terminated with this signal.


	*/
	SYNAPSE_REGISTER(exec_Start)
	{
		//user wants to queue it?
		if (msg.dst!=queueId && msg.isSet("queue"))
		{
			//forward to our single threaded session
			msg["originalSrc"]=msg.src;
			msg.dst=queueId;
			msg.src=defaultId;
			msg.send();
			return;
		}

		//src is set?
		if (msg.isSet("originalSrc"))
		{
			//SECURITY: only WE may set the source
			if (msg.src!=defaultId)
			{
				throw(synapse::runtime_error("You are not allowed to set the src-session."));
			}
			msg.src=msg["originalSrc"];
		}

		const int buffer_size=4049;
		char buffer[buffer_size];
		Cmsg out;
		if (msg.isSet("id"))
			out["id"]=msg["id"];
		out.dst=msg.src;

		//open the process
		FILE *fh;
		fh=popen(msg["cmd"].str().c_str(), "r");
		if (fh==NULL)
		{
			out.event="exec_Error";
			out["error"].str()=strerror(errno);
			out.send();
			return;
		}

		out.event="exec_Started";
		out.send();

		//read as long as there is data
		while (!feof(fh))
		{
			if (fgets(buffer, buffer_size, fh)==NULL)
			{
				if (!feof(fh))
				{
					out.event="exec_Error";
					out["error"].str()=strerror(ferror(fh));
					out.send();
					pclose(fh);
					return;
				}
			}
			else
			{
				out.event="exec_Data";
				out["data"].str()=buffer;
				out.send();
			}
		}

		//close and read exit status
		out.erase("data");
		int exit=pclose(fh);
		if (exit==-1)
		{
			out.event="exec_Error";
			out["error"].str()=strerror(errno);
			out.send();
			return;
		}
		else
		{
			out.event="exec_Ended";
			if (WIFEXITED(exit))
			{
				out["exit"]=WEXITSTATUS(exit);
			}
			if (WIFSIGNALED(exit))
			{
				out["signal"]=WTERMSIG(exit);
			}

			out.send();
			return;
		}
	}


	SYNAPSE_REGISTER(module_Shutdown)
	{
		shutdown=true;
	}

}

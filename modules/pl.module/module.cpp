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
The play list module.

This module can dynamicly generate playlists from directory's. It also can cache per path metadata.



*/


#include "synapse.h"
#include "cconfig.h"
#include <string.h>
#include <errno.h>
#include <map>

#include "exception/cexception.h"

#include "boost/filesystem.hpp"

namespace pl
{
	using namespace std;
	using namespace boost::filesystem;

	bool shutdown;
	int defaultId=-1;

	SYNAPSE_REGISTER(module_Init)
	{
		Cmsg out;
		shutdown=false;
		defaultId=msg.dst;

		//load config file
		synapse::Cconfig config;
		config.load("etc/synapse/pl.conf");

		out.clear();
		out.event="core_ChangeModule";
		out["maxThreads"]=1;
		out.send();

		out.clear();
		out.event="core_ChangeSession";
		out["maxThreads"]=1;
		out.send();

		out.clear();
		out.event="core_LoadModule";
		out["path"]="modules/http_json.module/libhttp_json.so";
		out.send();


		//tell the rest of the world we are ready for duty
		out.clear();
		out.event="core_Ready";
		out.send();

	}

	SYNAPSE_REGISTER(module_SessionStart)
	{
	}


	class Citer
	{

	};

	typedef map<string,Citer> CiterMap;
	CiterMap iterMap;


	/** Get current directory and file
		\param id Traverser id

	\par Replies pl_Entry:
		\param id Traverser id
		\param path Current path
		\param file Current file, selected according to search criteria
	*/
	SYNAPSE_REGISTER(pl_Current)
	{
	}

	/** Change selection/search criteria for files
		\param id Traverser id
		\param recurse Recurse level (-1 is infinite depth)
		\param fileOrder Order in which to traverse files (date, random, name)
		\param dirOrder  Order in which to directorys files (date, name)
		\param search	Search parameters for metadata or filename (details later!)

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_Mode)
	{

	}

	/** Select next directory entry in list
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_NextDir)
	{

	}


	/** Select previous entry directory in list
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_PreviousDir)
	{

	}

	/** Enters selected directory
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_EnterDir)
	{

	}

	/** Exits directory, selecting directory on higher up the hierarchy
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_ExitDir)
	{

	}

	/** Next song
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_Next)
	{

	}

	/** Previous song
		\param id Traverser id

	\par Replies pl_Entry:
	 */
	SYNAPSE_REGISTER(pl_Previous)
	{

	}




	SYNAPSE_REGISTER(module_Shutdown)
	{
		shutdown=true;
	}

}

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



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cmessageman.h"
#include <string>
#include <list>

using namespace std;

int main(int argc, char *argv[])
{

	//since CVAR is so crucial and complex, we selftest it first:
	if (!synapse::Cvar::selfTest())
	{
		ERROR("Cvar selftest failed. (this should not happen and is a compiler- or program bug!)");
		return(1);
	}

	{

		synapse::CmessageMan messageMan;

		if (argc>=2)
		{
			list<string> moduleNames;

			for (int i=1; i<argc; i++)
			{
				moduleNames.push_back(argv[i]);
			}
			return (messageMan.run("core", moduleNames));
		}
		else
		{
			INFO("Usage: ./synapse <module> [ <module2>, <...> ]\n");
			return (1);
		}
	}
}



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


/*
 * cconfig.cpp
 *
 *  Created on: Jul 28, 2010

 */

#include "cconfig.h"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/fstream.hpp"
#include "clog.h"
#include <errno.h>

namespace synapse
{
	using namespace std;
	using boost::filesystem::ofstream;
	using boost::filesystem::ifstream;

	Cconfig::Cconfig()
	{
		saved=false;
	}

	void Cconfig::save(path configPath)
	{
		//errors are handled with exceptions
		if (!saved || configPath!=savedConfigPath)
		{
			INFO("Saving config file "<< configPath);
			string s;
			toJsonFormatted(s);

			ofstream configStream;
			configStream.exceptions ( ofstream::eofbit | ofstream::failbit | ofstream::badbit );
			configStream.open(configPath);
			configStream << s;
			configStream.close();
			saved=true;
			savedConfigPath=configPath;
		}

	}

	bool Cconfig::isChanged()
	{
		return(!saved);
	}

	void Cconfig::changed()
	{
		saved=false;
	}

	void Cconfig::load(path configPath, bool merge)
	{
		INFO("Loading config file "<< configPath);
		//errors are handled with exceptions


		if (!merge)
			clear();


		ifstream configStream;
		configStream.exceptions ( ifstream::eofbit | ifstream::failbit | ifstream::badbit );
		configStream.open(configPath);
		string s((istreambuf_iterator<char>(configStream)), std::istreambuf_iterator<char>());
		configStream.close();
		fromJson(s);

		if (!merge)
			saved=true;

		savedConfigPath=configPath;
	}


}

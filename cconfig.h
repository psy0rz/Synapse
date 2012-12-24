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
 * cconfig.h
 *
 *  Created on: Jul 28, 2010

 */

#ifndef CCONFIG_H_
#define CCONFIG_H_

#include "cvar.h"
#include <string>
#include "boost/filesystem/operations.hpp"
#include <time.h>

namespace synapse
{
	using namespace std;
	using namespace boost::filesystem;
    

	class Cconfig : public Cvar
	{
		private:
//		time_t changeTime;
		bool saved;
		path savedConfigPath;

		public:
		Cconfig();

		void changed();
		bool isChanged();
//		time_t getChangeTime();
		void save(path configPath);
		void save();
		void load(path configPath, bool merge=false);

	};

}

#endif /* CCONFIG_H_ */

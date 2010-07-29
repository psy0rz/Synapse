/*
 * cconfig.h
 *
 *  Created on: Jul 28, 2010
 *      Author: psy
 */

#ifndef CCONFIG_H_
#define CCONFIG_H_

#include "cvar.h"
#include <string>
#include "boost/filesystem/operations.hpp"

namespace synapse
{
	using namespace std;
	using namespace boost::filesystem;


	class Cconfig : public Cvar
	{
		public:
		void save(path configPath);
		void load(path configPath, bool merge=false);

	};

}

#endif /* CCONFIG_H_ */

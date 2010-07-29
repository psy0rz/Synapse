/*
 * cconfig.cpp
 *
 *  Created on: Jul 28, 2010
 *      Author: psy
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


	void Cconfig::save(path configPath)
	{
		//errors are handled with exceptions

		string s;
		toJsonFormatted(s);

		ofstream configStream;
		configStream.exceptions ( ofstream::eofbit | ofstream::failbit | ofstream::badbit );
		configStream.open(configPath);
		configStream << s;
		configStream.close();

	}

	void Cconfig::load(path configPath, bool merge)
	{
		//errors are handled with exceptions

		if (!merge)
			clear();


		ifstream configStream;
		configStream.exceptions ( ifstream::eofbit | ifstream::failbit | ifstream::badbit );
		configStream.open(configPath);
		string s((istreambuf_iterator<char>(configStream)), std::istreambuf_iterator<char>());
		configStream.close();
		fromJson(s);
	}


}

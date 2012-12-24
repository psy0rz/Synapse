/*
 * utils.h
 *
 *  Created on: Jan 18, 2012
 *      Author: psy
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <string>
#include "cvar.h"

namespace utils
{
	using namespace std;
	using namespace synapse;

	//replace a bunch of regular expressions in a file.
	//the regex Cvar is a hasharray:
	// regex => replacement
	void regexReplaceFile(const string & inFilename, const string & outFilename,  synapse::CvarMap & regex);

	//return a random readable string, for use as key or uniq id
	string randomStr(int length);
}


#endif /* UTILS_H_ */

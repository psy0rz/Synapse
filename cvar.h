//
// C++ Interface: cvar
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CVAR_H
#define CVAR_H

#include "clog.h"
#include <string>
#include "boost/variant.hpp"
#include <map>
using namespace std;
using namespace boost;

/**
	@author 
*/

//this has to corospond with the variant order in the Cvar class below:
#define CVAR_LONG_DOUBLE 0
#define CVAR_STRING 1

class Cvar : public map<const string,Cvar >{
public:
    Cvar();

    ~Cvar();
 	void operator=(const string & value);
//	operator string ();
	operator string &();
	string & str();

 	void operator=(const long double & value);
	operator long double  ();
	string getPrint(string prefix);
	int which();
    bool isSet(string key);
	

private:
	
	variant <long double,string> value;
};

#endif

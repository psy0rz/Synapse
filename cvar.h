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
#include "json_spirit.h"

namespace synapse
{
	using namespace std;
	using namespace boost;
	using namespace json_spirit;

	/**
		@author
	*/

	//this has to corospond with the variant order in the Cvar class below:
	#define CVAR_EMPTY 0
	#define CVAR_MAP 1
	#define CVAR_LONG_DOUBLE 2
	#define CVAR_STRING 3
	#define CVAR_LIST 4

	typedef map< string,class Cvar> CvarMap;
	typedef list<class Cvar> CvarList;

	class Cvar {
	public:
		typedef CvarMap::iterator iterator;

		//common stuff
		int which();
		string getPrint(string prefix="");
		Cvar();
		~Cvar();
		void clear();
		bool isEmpty();

		//CVAR_STRING stuff
		Cvar(const string & value);
	//    Cvar(const char * value);
		void operator=(const string & value);
	 //	void operator=(const char * value);
		operator string &();
		string & str();

		//CVAR_LONG_DOUBLE stuff
		void operator=(const long double & value);
		Cvar(const long double & value);
		operator long double  ();


		//CVAR_MAP and CVAR_LIST stuff
		const int size();

		//CVAR_MAP stuff
		void operator=(CvarMap & value);
		operator CvarMap & ();
		Cvar & operator [](const string & key);
		bool isSet(const char * key);
		CvarMap::iterator begin();
		CvarMap::iterator end();
		void erase(const char * key);
		CvarMap & map();

		//CVAR_LIST stuff
		void operator=(CvarList & value);
		CvarList & list();

		//json conversion stuff
		void toJson(string & jsonStr);
		void toJsonFormatted(string & jsonStr);
		void fromJson(string & jsonStr);


	protected:

		//json spirit specific conversion stuff, used internally
		//We might decide to use a safer/faster json parser in the future!
		void toJsonSpirit(Value &spiritValue);
		void fromJsonSpirit(Value &spiritValue);
		void readJsonSpirit(const string & jsonStr, Value & spiritValue);
	
	private:
		variant <void *,CvarMap , long double, string, CvarList> value;
	};
}

typedef synapse::Cvar Cvar;


#endif

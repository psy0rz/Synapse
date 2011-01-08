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
		typedef CvarList::iterator iteratorList;

		//common stuff
		int which();
		string getPrint(string prefix="");
		Cvar();
		~Cvar();
		void clear();
		bool isEmpty();
		void castError(const char *txt);

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
		bool isSet(const string & key);
		CvarMap::iterator begin();
		CvarMap::iterator end();
		void erase(const char * key);
		CvarMap & map();

		//CVAR_LIST stuff
		void operator=(CvarList & value);
		CvarList & list();

		//comparison
		bool operator==( Cvar & other);
		bool operator!=( Cvar & other);


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
typedef synapse::CvarList CvarList;


#endif

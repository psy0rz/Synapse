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













#include "cvar.h"
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "clog.h"
#include <sstream>

#include "exception/cexception.h"


namespace synapse
{

/// COMMON STUFF

Cvar::Cvar()
{
	clear();
}


Cvar::~Cvar()
{
	
}

void Cvar::clear()
{
	value=(void *)NULL;
}

bool Cvar::isEmpty()
{
	return (value.which()==CVAR_EMPTY);
}

int Cvar::which()
{
	return (value.which());
}


string Cvar::getPrint(string prefix)
{
	stringstream print;
	switch (value.which())
	{
		case CVAR_EMPTY:
			print << "(empty)";
			break;
		case CVAR_LONG_DOUBLE:
			print << (long double)(*this) << " (long double)";
			break;
		case CVAR_STRING:
			print << (string)(*this) << " (string)";
			break;
		case CVAR_MAP:
			//show all keys of the map, and show the values recursively
			if (begin()!=end())
			{
				print << "(map:)";
				for (CvarMap::iterator varI=begin(); varI!=end(); varI++)
				{
					print << endl;
					print << prefix << varI->first << " = ";
					print << varI->second.getPrint(prefix+" |");
				}
			}
			else
			{
				print << "(empty map)";
			}

			break;
		case CVAR_LIST:
			//show all items of the list recursively
			if (list().begin()!=list().end())
			{
				print << "(list:)";
				for (CvarList::iterator varI=list().begin(); varI!=list().end(); varI++)
				{
					print << endl;
					print << prefix << varI->getPrint(prefix+" |");
				}
			}
			else
			{
				print << "(empty list)";
			}

			break;
		default:
			print << "? (unknown type)";
			break;
	}

	return print.str();
}

bool Cvar::operator==( Cvar & other)
{

	//wrong type
	if (value.which()!=other.value.which())
		return (false);


	switch (value.which())
	{
		case CVAR_EMPTY:
			return (true);
			break;
		case CVAR_LONG_DOUBLE:
			return ((long double)(*this)==(long double)other);
			break;
		case CVAR_STRING:
			return ((string)(*this)==(string)(other));
			break;
		case CVAR_MAP:
			//wrong count?
			if (((CvarMap)(*this)).size()!=((CvarMap)other).size())
				return (false);

			//recursively compare keys and values
			for (CvarMap::iterator varI=begin(); varI!=end(); varI++)
			{
				//key doesnt exists in other?
				if (!other.isSet(varI->first))
					return (false);

				//compare values (this recurses)
				if (varI->second!=other[varI->first])
					return (false);
			}
			return (true);
			break;
		case CVAR_LIST:
			//wrong count?
			if (list().size()!=other.list().size())
				return (false);

			{
				//recursively compare values (order has to be the same as well)
				CvarList::iterator otherVarI=other.list().begin();
				for (CvarList::iterator varI=list().begin(); varI!=list().end(); varI++)
				{
					//compare values (this recurses)
					if ((*varI)!=(*otherVarI))
						return false;
					otherVarI++;
				}
			}
			return (true);
			break;
		default:
			castError("trying to compare unknown types?");
			return (false);
			break;
	}
}

bool Cvar::operator!=( Cvar & other)
{
	return !(*this == other);
}




/// CVAR_STRING stuff
Cvar::Cvar(const string & value)
{
	this->value=value;
}

// Cvar::Cvar(const char * value)
// {
// 	this->value=string(value);
// }

string & Cvar::operator=(const string & value)
{
	this->value=value;
	return((string &)(*this));
}


void Cvar::castError(const char * txt)
{
	string s(txt);
	s+=": "+getPrint();
	throw(synapse::runtime_error(s.c_str()));
}

Cvar::operator string & ()
{
	if (value.which()==CVAR_LONG_DOUBLE)
	{
		try
		{
			//DEB("Cvar " << this << ": converting CVAR_LONG_DOUBLE to CVAR_STRING");
			value=lexical_cast<string>(get<long double>(value));
		}
		catch(...)
		{
			castError("casting error while converting to CVAR_STRING");
		}
	}
	else if (value.which()==CVAR_STRING)
	{
		//native type, dont need to do anything
	}
	else if (value.which()==CVAR_EMPTY)
	{
		//change it from really empty, to an empty string
		//DEB("Cvar " << this << ": converting CVAR_EMPTY to CVAR_STRING");
		value="";
	}
	else
	{
		castError("cannot convert this to CVAR_STRING");
	}

	return (get<string&>(value));
}

string & Cvar::str()
{
	return((string &)(*this));
}

bool Cvar::operator==(const string & other)
{
	return ((string)(*this)==(other));
}

bool Cvar::operator!=(const string & other)
{
	return ((string)(*this)!=(other));
}

//bool Cvar::operator==(const char * other)
//{
//	return ((string)(*this)==other);
//}
//
//bool Cvar::operator!=(const char * other)
//{
//	return ((string)(*this)!=other);
//}


/// CVAR_LONG_DOUBLE stuff

long double & Cvar::operator=(const long double & value)
{
	this->value=value;
	return((long double &)(*this));
}

 Cvar::Cvar(const long double & value)
{
	this->value=value;
}

Cvar::operator long double &()
{
	if (value.which()==CVAR_LONG_DOUBLE)
	{
		//native type, dont change anything
	}
	else if (value.which()==CVAR_STRING)
	{
		try
		{
			//convert string to long double
			//DEB("Cvar " << this << ": converting CVAR_STRING to CVAR_LONG_DOUBLE");
			value=lexical_cast<long double>(get<string>(value));
		}
		catch(...)
		{
			castError("casting error while converting to CVAR_LONG_DOUBLE");
		}
	}
	else if (value.which()==CVAR_EMPTY)
	{
		//change it from empty to 0
		//DEB("Cvar " << this << ": converting CVAR_EMPTY to CVAR_LONG_DOUBLE");
		value=0;
	}
	else
	{
		castError("cannot convert this to CVAR_LONG_DOUBLE");
	}
	return (get<long double&>(value));
}

//bool Cvar::operator==(long double & other)
//{
//	return ((long double)(*this)==other);
//
//}


/// CVAR_MAP stuff
Cvar::operator CvarMap & ()
{
	if (value.which()==CVAR_EMPTY)
	{
		//DEB("Cvar " << this << ": converting CVAR_EMPTY to CVAR_MAP"); 
		value=CvarMap();
	}
	else if (value.which()!=CVAR_MAP)
	{
		castError("cannot convert this to CVAR_MAP");
	}
	return (get<CvarMap&>(value));
}

CvarMap & Cvar::map()
{
	return ((CvarMap &)(*this));

}


Cvar & Cvar::operator[] (const string & key)
{
	return (((CvarMap&)(*this))[key]);
}

CvarMap::iterator Cvar::begin()
{
	return ((CvarMap &)(*this)).begin();
}

CvarMap::iterator Cvar::end()
{
	return ((CvarMap &)(*this)).end();
}

bool Cvar::isSet(const char * key)
{
	return(
		((CvarMap&)(*this)).find(key)!=((CvarMap&)(*this)).end()
	);
}

bool Cvar::isSet(const string & key)
{
	return(
		((CvarMap&)(*this)).find(key)!=((CvarMap&)(*this)).end()
	);
}

void Cvar::erase(const char * key)
{
	CvarMap::iterator I=((CvarMap&)(*this)).find(key);
	if (I!=((CvarMap&)(*this)).end())
		((CvarMap&)(*this)).erase(I);
}

/// CVAR_LIST stuff
Cvar::operator CvarList & ()
{
	if (value.which()==CVAR_EMPTY)
	{
		value=CvarList();
	}
	else if (value.which()!=CVAR_LIST)
	{
		castError("cannot convert this to CVAR_LIST");
	}
	return (get<CvarList&>(value));
}


CvarList & Cvar::list()
{
	return ((CvarList &)(*this));
}





/// JSON stuff

/** Recursively converts a json_spirit Value to this Cvar
*/
void Cvar::fromJsonSpirit(Value &spiritValue)
{
	switch(spiritValue.type())
	{
		case(null_type):
			clear();
			break;
		case(str_type):
			value=spiritValue.get_str();
			break;
		case(real_type):
			value=spiritValue.get_real();
			break;
		case(int_type):
			value=spiritValue.get_int();
			break;
		case(bool_type):
			value=spiritValue.get_bool();
			break;
		case(obj_type):
			value=CvarMap();
			//convert the Object(string,Value) pairs to a CvarMap 
			for (Object::iterator ObjectI=spiritValue.get_obj().begin(); ObjectI!=spiritValue.get_obj().end(); ObjectI++)
			{
				//recurse to convert the json_spirit pair into a CvarMap key->item.
				map()[ObjectI->name_].fromJsonSpirit(ObjectI->value_);
			}
			break;
		case(array_type):
			value=CvarList();
			//convert the Array items to CvarList
			for (Array::iterator ArrayI=spiritValue.get_array().begin(); ArrayI!=spiritValue.get_array().end(); ArrayI++)
			{
				//recurse to convert the json_spirit value item into a CvarList item
				Cvar v;
				v.fromJsonSpirit(*ArrayI);
				list().push_back(v);
			}
			break;
		default:
			DEB("Unknown jsonspirit type: " << spiritValue.type());
			castError("Cant convert this json spirit data to Cvar?");
			break;
	}
}


/** Recursively converts this Cvar to a json_spirit Value.
*/
void Cvar::toJsonSpirit(Value &spiritValue)
{
	int i;
	double d;

	switch(value.which())
	{
		case(CVAR_EMPTY):
			spiritValue=Value();
			break;
		case(CVAR_STRING):
			spiritValue=get<string&>(value);
			break;
		case(CVAR_LONG_DOUBLE):
			//work around to prevent numbers like 1.00000000000:
			i=(get<long double>(value));
			d=(get<long double>(value));
			if (d==i)
				spiritValue=i;
			else
				spiritValue=d;

			break;
		case(CVAR_MAP):
			//convert the CvarMap to a json_spirit Object with (String,Value) pairs 
			spiritValue=Object();
			for (CvarMap::iterator varI=begin(); varI!=end(); varI++)
			{
				Value subValue;
				//recurse to convert the map-value of the CvarMap into a json_spirit Value:
				varI->second.toJsonSpirit(subValue);
				
				//push the resulting Value onto the json_spirit Object
				spiritValue.get_obj().push_back(Pair(
					varI->first,
					subValue
				));
			}
			break;
		case(CVAR_LIST):
			//convert the CvarList to a json_spirit Array with (String,Value) pairs 
			spiritValue=Array();
			for (CvarList::iterator varI=list().begin(); varI!=list().end(); varI++)
			{
				Value subValue;
				//recurse to convert the list item of the CvarList into a json_spirit Value:
				varI->toJsonSpirit(subValue);
				
				//push the resulting Value onto the json_spirit Object
				spiritValue.get_array().push_back(subValue);
			}
			break;
		default:
			castError("Cant convert this Cvar type to json spirit?");
			break;
	}
}


/** converts this Cvar recursively into a json string
*/
void Cvar::toJson(string & jsonStr)
{
	Value spiritValue;
	toJsonSpirit(spiritValue);

	jsonStr=write(spiritValue);

}

//"better" version of json read that throws nicer exceptions
void Cvar::readJsonSpirit(const string & jsonStr, Value & spiritValue)
{
	//TODO:how safe is it actually to let json_spirit parse untrusted input? (regarding DoS, buffer overflows, etc)
	if (!json_spirit::read(jsonStr, spiritValue))
	{
		//this function is 3 times slower, but gives a nice exception with an error message
		try
		{
			json_spirit::read_or_throw(jsonStr, spiritValue);
		}
		catch(json_spirit::Error_position e)
		{
			//reformat the exception to something generic:
			stringstream s;
			s << "JSON parse error at line " << e.line_<< ", column " << e.column_ << ": " << e.reason_;
			throw(synapse::runtime_error(s.str()));
		}
	}


}


void Cvar::toJsonFormatted(string & jsonStr)
{
	Value spiritValue;
	toJsonSpirit(spiritValue);

	jsonStr=write_formatted(spiritValue);

}


void Cvar::fromJson(string & jsonStr)
{
	//parse json input
	Value spiritValue;
	readJsonSpirit(jsonStr,spiritValue);
	fromJsonSpirit(spiritValue);

}

//test functionality of Cvar class, returns false on failure.
//Its wise to also  run this small and fast test in the non-debug version, to detect strange problems with compiler optimizations or different compiler version.
bool Cvar::selfTest()
{
#define TEST(condition) if (!(condition)) return (false)
//tests if operation throws an exception.
//NOTE: dont forget to check if the value is still what it is supposed to be, AFTER the throw.
#define TESTTHROW(operation) \
		{ \
			bool ok=false; \
			try \
			{ \
				operation ; \
			} \
			catch (const synapse::runtime_error& e) \
			{ \
				ok=true; \
				DEB("exception: " << e.what()); \
			} \
			catch(...) \
			{ \
				ok=true; \
			} \
			if (!ok) ERROR("did not throw exception!"); \
			TEST(ok); \
		}



	{
		DEB("test: Selftesting CVAR_LONG_DOUBLE");

		DEB("test: empty long value");
		Cvar e;
		TEST((int)e==0);

		DEB("test: use long as boolean");
		e=1;
		TEST(e);

		DEB("test: long construct");
		Cvar a(2.12);
		TEST(a==2.12);

		DEB("test: long assignment");
		TEST((a=3.13) == 3.13);
		TEST(a==3.13);


		DEB("test: long assignment reference")
		Cvar b;
		TEST((a=b=5.15) == 5.15);
		TEST(a==5.15);
		TEST(b==5.15);

		DEB("test: int to string");
		a=6;
		TEST((string)a=="6");

		DEB("test: long to string");
		a=6.14;
		TEST((string)a=="6.13999999999999968026");
		TEST(a.str()=="6.13999999999999968026");

		DEB("test: long to map");
		a=7;
		TESTTHROW((CvarMap)a);
		TESTTHROW(a.map());
		TEST(a==7);

		DEB("test: long to list");
		a=8;
		TESTTHROW(a.list());
		TEST(a==8);

		DEB("test long compare");
		Cvar c;
		c=a;
		TEST(a==c);

		DEB("test: long re-assign string");
		a=9;
		TEST(a==9);
		a="test";
		TEST(a.str()=="test");

		DEB("test: long re-assign list");
		a=9;
		TEST(a==9);
		Cvar l;
		l.list();
		a=l;
		TEST(a.list().size()==0);

		DEB("test: long re-assign map");
		a=9;
		TEST(a==9);
		Cvar m;
		m.map();
		a=m;
		TEST(a.map().size()==0);

		DEB("test: basic arithmetic");
		a=5;
		a=a+1;
		a=a-2;
		TEST(a==4);
	}

	{
		DEB("test: Selftesting CVAR_STRING");

		DEB("test: empty string value");
		Cvar e;
		TEST(e=="");

		DEB("test: use string as boolean");
		e="1";
		TEST(e);
		TEST(e=="1");

		DEB("test: use invalid string as boolean");
		e="blah";
		TESTTHROW(if (e););
		TEST(e=="blah");

		DEB("test: string construct from const char");
		Cvar a("first");
		TEST(a=="first");

		DEB("test: const char assignment");
		TEST((a="second") == "second");
		TEST(a=="second");

		DEB("test: const char assignment reference")
		Cvar b;
		TEST((a=b="third") == "third");
		TEST(a=="third");
		TEST(b=="third");

		DEB("test: valid string number to int");
		a="6";
		TEST(a==6);

		DEB("test: invalid string number to int");
		a="blah";
		TESTTHROW(a=a+6);
		TEST(a=="blah");

		DEB("test: string to map");
		a="fourth";
		TESTTHROW((CvarMap)a);
		TESTTHROW(a.map());
		TEST(a=="fourth");

		DEB("test: string to list");
		a="fifth";
		CvarList d;
		TESTTHROW(d=a);
		TESTTHROW(a.list());
		TEST(a=="fifth");

		DEB("test string compare");
		Cvar c;
		c=a;
		TEST(a==c);

		DEB("test: string re-assign long");
		a="sixth";
		TEST(a=="sixth");
		a=7;
		TEST(a==7);

		DEB("test: string re-assign list");
		a="sixth";
		TEST(a=="sixth");
		Cvar l;
		l.list();
		a=l;
		TEST(a.list().size()==0);

		DEB("test: string re-assign map");
		a="sixth";
		TEST(a=="sixth");
		Cvar m;
		m.map();
		a=m;
		TEST(a.map().size()==0);

	}


	{
		DEB("test: Selftesting CVAR_LIST");

		DEB("test: create basic list");
		Cvar a;
		a.list().push_back(Cvar("one"));
		a.list().push_back(2);
		TEST(a.list().front()=="one");
		TEST(a.list().back()==2);

		DEB("test: list to long");
		TESTTHROW(a=a+1);
		TEST(a.list().back()==2);

		DEB("test: list to string");
		TESTTHROW(a.str());
		TEST(a.list().back()==2);

		DEB("test: list to map");
		TESTTHROW(a.map());
		TEST(a.list().back()==2);

		DEB("test list compare");
		Cvar c;
		c=a;
		TEST(a==c);

		DEB("test: list re-assign long");
		a.clear();
		a.list();
		a=5;
		TEST(a==5);

		DEB("test: list re-assign string");
		a.clear();
		a.list();
		a="test";
		TEST(a=="test");

		DEB("test: list re-assign map");
		a.clear();
		a.list();
		Cvar b;
		b.map();
		a=b;
		TEST(a.map().size()==0);


	}

	{
		DEB("test: Selftesting CVAR_MAP");

		DEB("test: create basic map");
		Cvar a;
		a["first"]=1;
		a["second"]="two";
		TEST(a["first"]==1);
		TEST(a["second"]=="two");

		DEB("test: map to long");
		TESTTHROW(a=a+1);
		TEST(a["first"]==1);

		DEB("test: map to string");
		TESTTHROW(a.str());
		TEST(a["first"]==1);

		DEB("test: map to list");
		TESTTHROW(a.list());
		TEST(a["first"]==1);

		DEB("test map compare");
		Cvar c;
		c=a;
		TEST(a==c);

		DEB("test: map re-assign long");
		a.clear();
		a.map();
		a=5;
		TEST(a==5);

		DEB("test: map re-assign string");
		a.clear();
		a.map();
		a="test";
		TEST(a=="test");

		DEB("test: map re-assign list");
		a.clear();
		a.map();
		Cvar b;
		b.list();
		a=b;
		TEST(a.list().size()==0);


	}

	{
		DEB("test: Selftesting Cvar complex data structures");

		DEB("test: create complex structure");
		Cvar a;
		a["empty"];
		a["number"]=1;
		a["list"].list().push_back(1);
		a["list"].list().push_back(Cvar());
		a["string"]="text";
		a["map"]["number2"]=2;
		a["map"]["list2"].list().push_back(2);
		a["map"]["string2"]="text2";
		a["list"].list().back()["submap"].list().push_back(3);
		DEB("test: complex map layout:" << a.getPrint());

		DEB("test: convert to json and back");
		string s;
		a.toJson(s);
		Cvar b;
		b.fromJson(s);

		DEB("test: complex compare json result");
		TEST(a==b);
		//make a deep but small change:
		b["list"].list().back()["submap"].list().back()=4;
		TEST(a!=b);
		//change it back:
		b["list"].list().back()["submap"].list().back()=3;
		TEST(a==b);

		DEB("test: convert to formatted json and back");
		a.toJsonFormatted(s);
		b.fromJson(s);
		TEST(a==b);

	}

	DEB("test: all Cvar tests OK");
	return(true);
}


}

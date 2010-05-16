//
// C++ Implementation: cvar
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "cvar.h"
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "clog.h"

/// COMMON STUFF

Cvar::Cvar()
{

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


/*!
    \fn Cvar::print(string s)
 */
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
//			print "map: " << endl;
			if (begin()!=end())
			{
				for (Cvar::iterator varI=begin(); varI!=end(); varI++)
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
		default:
			print << "? (unknown type)";
			break;
	}

	return print.str();
}




/// CVAR_STRING stuff

/*!
    \fn Cvar::operator=
 */
void Cvar::operator=(const string & value)
{
	this->value=value;
}


Cvar::operator string & ()
{
	try
	{
		if (value.which()==CVAR_LONG_DOUBLE)
		{
			//DEB("Cvar " << this << ": converting CVAR_LONG_DOUBLE to CVAR_STRING");
			//convert value to a permanent string
			value=lexical_cast<string>(get<long double>(value));
			return (get<string&>(value));
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
			WARNING("Cvar " << this << ": Cant convert from data-type " << value.which() << " to CVAR_STRING, replacing data with ''");
			value="";
		}
	}
	catch(...)
	{
		WARNING("Cvar " << this << ": Casting error while converting CVAR_LONG_DOUBLE to CVAR_STRING, replacing data with ''");
		value="";
	}

	return (get<string&>(value));
}

string & Cvar::str()
{
	return((string &)*this);	
}

void Cvar::operator=(const long double & value)
{
	this->value=value;
}

/// CVAR_LONG_DOUBLE stuff

Cvar::operator long double()
{
	try
	{
		if (value.which()==CVAR_LONG_DOUBLE)
		{
			//native type, dont change anything
		}
		else if (value.which()==CVAR_STRING)
		{
			//convert string to long double
			//DEB("Cvar " << this << ": converting CVAR_STRING to CVAR_LONG_DOUBLE"); 
			return (lexical_cast<long double>(get<string>(value)));
		}
		else if (value.which()==CVAR_EMPTY)
		{
			//change it from empty to 0
			//DEB("Cvar " << this << ": converting CVAR_EMPTY to CVAR_LONG_DOUBLE"); 
			value=0;
		}
		else
		{
			WARNING("Cvar " << this << ": Cant convert from data-type " << value.which() << " to CVAR_LONG_DOUBLE, changing it to 0");
			value=0;
		}
	}
	catch(...)
	{
		WARNING("Cvar " << this << ": Casting error while converting CVAR_STRING to CVAR_LONG_DOUBLE, replacing it with 0");
		value=0;
	}
	return (get<long double>(value));
}

///CVAR_MAP and CVAR_LIST stuff
const int Cvar::size()
{
	switch (value.which())
	{
		case CVAR_MAP:
			return (((CvarMap&)(*this)).size());
		case CVAR_LIST:
			return (((CvarList&)(*this)).size());
		default:
			return(0);
	}
}


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
		WARNING("Cvar " << this << ": Cant convert from data-type " << value.which() << " to CVAR_MAP, creating empty map");
		value=CvarMap();
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

/*!
    \fn Cvar::isSet(string key)
 */
bool Cvar::isSet(const char * key)
{
	return(
		((CvarMap&)(*this)).find(key)!=((CvarMap&)(*this)).end()
	);
}


void Cvar::erase(const char * key)
{
	((CvarMap&)(*this)).erase(
		((CvarMap&)(*this)).find(key)
	);
}

/// CVAR_LIST stuff

CvarList & Cvar::list()
{
	return (get<CvarList&>(value));

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
		case(obj_type):
			value=CvarMap();
			//convert the Object(string,Value) pairs to a CvarMap 
			for (Object::iterator ObjectI=spiritValue.get_obj().begin(); ObjectI!=spiritValue.get_obj().end(); ObjectI++)
			{
				//recurse to convert the map-value of the CvarMap into a json_spirit Value:
				map()[ObjectI->name_].fromJsonSpirit(ObjectI->value_);
			}
			break;
		default:
			WARNING("Cant convert json spirit variable type " << spiritValue.type() << " to Cvar");
			break;
	}
}


/** Recursively converts this Cvar to a json_spirit Value.
*/
void Cvar::toJsonSpirit(Value &spiritValue)
{
	switch(value.which())
	{
		case(CVAR_EMPTY):
			spiritValue=Value();
			break;
		case(CVAR_STRING):
			spiritValue=get<string&>(value);
			break;
		case(CVAR_LONG_DOUBLE):
			spiritValue=(double)(get<long double>(value));
			break;
		case(CVAR_MAP):
			//convert the CvarMap to a json_spirit Object with (String,Value) pairs 
			spiritValue=Object();
			for (CvarMap::iterator varI=begin(); varI!=end(); varI++)
			{
				Value subValue;
				//recurse to convert the map-value of the CvarMap into a json_spirit Value:
				//Cvar2Value(varI->second, subValue);
				varI->second.toJsonSpirit(subValue);
				
				//push the resulting Value onto the json_spirit Object
				spiritValue.get_obj().push_back(Pair(
					varI->first,
					subValue
				));
			}
			break;
		default:
			WARNING("Cant convert Cvar type " << value.which() << " to json spirit.");
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

void Cvar::toJsonFormatted(string & jsonStr)
{
	Value spiritValue;
	toJsonSpirit(spiritValue);

	jsonStr=write_formatted(spiritValue);

}

bool Cvar::fromJson(string & jsonStr)
{

	//parse json input
	try
	{
		Value spiritValue;
		//TODO:how safe is it actually to let json_spirit parse untrusted input? (regarding DoS, buffer overflows, etc)
		json_spirit::read(jsonStr, spiritValue);
		fromJsonSpirit(spiritValue);
		return true;
	}
	catch(...)
	{
		return false;
	}
}

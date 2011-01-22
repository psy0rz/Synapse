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




/// CVAR_STRING stuff


Cvar::Cvar(const string & value)
{
	this->value=value;
}

// Cvar::Cvar(const char * value)
// {
// 	this->value=string(value);
// }

void Cvar::operator=(const string & value)
{
	this->value=value;
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
			//convert value to a permanent string
			value=lexical_cast<string>(get<long double>(value));
			return (get<string&>(value));
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
	return((string &)*this);	
}


/// CVAR_LONG_DOUBLE stuff

void Cvar::operator=(const long double & value)
{
	this->value=value;
}

 Cvar::Cvar(const long double & value)
{
	this->value=value;
}

Cvar::operator long double()
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
			return (lexical_cast<long double>(get<string>(value)));
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

CvarList & Cvar::list()
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


/// COMPARISON STUFF
bool Cvar::operator==( Cvar & other)
{

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
			if (((CvarList)(*this)).size()!=((CvarList)other).size())
				return (false);

			{
				//recursively compare values (order has to be the same as well)
				CvarList::iterator otherVarI;
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

}

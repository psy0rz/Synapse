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
	switch (which())
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
			DEB("Cvar " << this << ": converting CVAR_LONG_DOUBLE to CVAR_STRING");
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
			DEB("Cvar " << this << ": converting CVAR_EMPTY to CVAR_STRING"); 
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
			DEB("Cvar " << this << ": converting CVAR_STRING to CVAR_LONG_DOUBLE"); 
			return (lexical_cast<long double>(get<string>(value)));
		}
		else if (value.which()==CVAR_EMPTY)
		{
			//change it from empty to 0
			DEB("Cvar " << this << ": converting CVAR_EMPTY to CVAR_LONG_DOUBLE"); 
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



/// CVAR_MAP stuff
Cvar::operator CvarMap & ()
{
	if (value.which()==CVAR_EMPTY)
	{
		DEB("Cvar " << this << ": converting CVAR_EMPTY to CVAR_MAP"); 
		value=CvarMap();
	}
	else if (value.which()!=CVAR_MAP)
	{
		WARNING("Cvar " << this << ": Cant convert from data-type " << value.which() << " to CVAR_MAP, creating empty map");
		value=CvarMap();
	}
	return (get<CvarMap&>(value));
}


Cvar & Cvar::operator[] (const string & key)
{
	return (((CvarMap&)(*this))[key]);
}

Cvar::iterator Cvar::begin()
{
	return ((CvarMap &)(*this)).begin();
}

Cvar::iterator Cvar::end()
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

const int Cvar::size()
{
	return (((CvarMap&)(*this)).size());
}

void Cvar::erase(const char * key)
{
	((CvarMap&)(*this)).erase(
		((CvarMap&)(*this)).find(key)
	);
}


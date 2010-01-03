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

Cvar::Cvar()
{
}


Cvar::~Cvar()
{
}




/*!
    \fn Cvar::operator=
 */
void Cvar::operator=(const string & value)
{
	this->value=value;
}

// Cvar::operator string ()
// {
// 	try
// 	{
// 		if (value.which()==CVAR_LONG_DOUBLE)
// 			return (lexical_cast<string>(get<long double>(value)));
// 		else if (value.which()==CVAR_STRING)
// 			return (get<string>(value));
// 		else
// 			return (string(""));
// 	}
// 	catch(exception e)
// 	{
// 		return ("");
// 	}
// }

Cvar::operator string & ()
{
	try
	{
		if (value.which()==CVAR_LONG_DOUBLE)
		{
			//convert value to a permanent string
			value=lexical_cast<string>(get<long double>(value));
		}
	}
	catch(exception e)
	{
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

Cvar::operator long double()
{
	try
	{
		if (value.which()==CVAR_LONG_DOUBLE)
			return (get<long double>(value));
		else if (value.which()==CVAR_STRING)
			return (lexical_cast<long double>(get<string>(value)));
		else
			return 0;		
	}
	catch(exception e)
	{
		return 0;
	}
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
	for (Cvar::iterator varI=begin(); varI!=end(); varI++)
	{
		//asume that a user doesnt apply a value in case of another dimention:
		if (!varI->second.empty())
		{
			print << endl << prefix << varI->first << " :";
			print << varI->second.getPrint(prefix+" |");
		}
		else if (varI->second.which()==CVAR_LONG_DOUBLE)
			print << endl << prefix << varI->first << " = " << (long double)varI->second;
		else if (varI->second.which()==CVAR_STRING)
			print << endl << prefix << varI->first << " = " << (string)varI->second;
		else
			print << endl << prefix << varI->first << " ? ";
	}
	return print.str();
}


/*!
    \fn Cvar::isSet(string key)
 */
bool Cvar::isSet(string key)
{
	return (find(key)!=end());
}

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


//          Copyright John W. Wilkinson 2007 - 2009.
// Distributed under the MIT License, see accompanying file LICENSE.txt

// json spirit version 4.03

#include "json_spirit_writer.h"
#include "json_spirit_writer_template.h"

void json_spirit::write( const Value& value, std::ostream& os )
{
    write_stream( value, os, false );
}

void json_spirit::write_formatted( const Value& value, std::ostream& os )
{
    write_stream( value, os, true );
}

std::string json_spirit::write( const Value& value )
{
    return write_string( value, false );
}

std::string json_spirit::write_formatted( const Value& value )
{
    return write_string( value, true );
}

#ifndef BOOST_NO_STD_WSTRING

void json_spirit::write( const wValue& value, std::wostream& os )
{
    write_stream( value, os, false );
}

void json_spirit::write_formatted( const wValue& value, std::wostream& os )
{
    write_stream( value, os, true );
}

std::wstring json_spirit::write( const wValue&  value )
{
    return write_string( value, false );
}

std::wstring json_spirit::write_formatted( const wValue&  value )
{
    return write_string( value, true );
}

#endif

void json_spirit::write( const mValue& value, std::ostream& os )
{
    write_stream( value, os, false );
}

void json_spirit::write_formatted( const mValue& value, std::ostream& os )
{
    write_stream( value, os, true );
}

std::string json_spirit::write( const mValue& value )
{
    return write_string( value, false );
}

std::string json_spirit::write_formatted( const mValue& value )
{
    return write_string( value, true );
}

#ifndef BOOST_NO_STD_WSTRING

void json_spirit::write( const wmValue& value, std::wostream& os )
{
    write_stream( value, os, false );
}

void json_spirit::write_formatted( const wmValue& value, std::wostream& os )
{
    write_stream( value, os, true );
}

std::wstring json_spirit::write( const wmValue&  value )
{
    return write_string( value, false );
}

std::wstring json_spirit::write_formatted( const wmValue&  value )
{
    return write_string( value, true );
}

#endif

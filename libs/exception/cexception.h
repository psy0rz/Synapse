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



#ifndef CEXCEPTION_H
#define CEXCEPTION_H
#include <stdexcept>
#include <string>

namespace synapse
{
	class runtime_error : public std::runtime_error
	{
		private:
		std::string mTrace;

		public:
		runtime_error( const std::string &err );
		const char* getTrace() const throw();

		virtual
	    ~runtime_error() throw();

	};
}


#endif



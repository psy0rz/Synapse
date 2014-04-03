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


/*
 * csession.h
 *
 *  Created on: Jul 14, 2010

 */

#ifndef CSESSION_H_
#define CSESSION_H_
#include "typedefs.h"

namespace asterisk
{
	using namespace std;
	using namespace boost;


	//sessions: every synapse session has a corresponding session object here
	class Csession
	{
		private:
		int id;
		CdevicePtr devicePtr;
		CserverPtr serverPtr;
		bool admin;
		bool authorized;

		public:

		Csession(int id);
		void authorize(CserverPtr serverPtr, CdevicePtr devicePtr);
		void deauthorize();
		bool isAuthorized();
		bool isAdmin();
		int getId();
		CdevicePtr getDevicePtr();
		CserverPtr getServerPtr();
		string getStatus(string prefix);
	};


};

#endif /* CSESSION_H_ */

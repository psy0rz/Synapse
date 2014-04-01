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



#ifndef CGROUP_H_
#define CGROUP_H_

#include <boost/shared_ptr.hpp>

namespace asterisk
{
	using namespace boost;
	using namespace std;

	typedef shared_ptr<class Cgroup> CgroupPtr;

}

#include "csession.h"

namespace asterisk
{
	using namespace boost;
	using namespace std;

	//groups: most times a tennant is considered a group.
	//After authenticating, a session points to a device
	//All devices of a specific tennant also point a group
	//events are only sent to Csessions that are member of the same group as the corresponding device.
	class Cgroup
	{
		private:
		string id;

		public:
		Cgroup();
		void setId(string id);
		string getId();
		void send(Cmsg & msg);
		string getStatus(string prefix);
	};


}

#endif /* CGROUP_H_ */

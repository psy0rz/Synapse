/*
 * cgroup.h
 *
 *  Created on: Jul 14, 2010
 *      Author: psy
 */

#ifndef CGROUP_H_
#define CGROUP_H_

#include <boost/shared_ptr.hpp>
#include <string>
#include "cmsg.h"

namespace asterisk
{
	using namespace boost;
	using namespace std;
	using namespace asterisk;
	typedef shared_ptr<class Cgroup> CgroupPtr;
	typedef map<string, CgroupPtr> CgroupMap;
}

#include "csession.h"

namespace asterisk
{
	using namespace boost;
	using namespace std;
	using namespace asterisk;

	//groups: most times a tennant is considered a group.
	//After authenticating, a session points to a group.'
	//All devices of a specific tennant also point to this group
	//events are only sent to Csessions that are member of the same group as the corresponding device.
	class Cgroup
	{
		private:
		string id;

		public:
		Cgroup();
		void setId(string id);
		string getId();
		void send(CsessionMap & sessionMap, Cmsg & msg);
		string getStatus(string prefix);
	};


}

#endif /* CGROUP_H_ */

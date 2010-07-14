/*
 * csession.h
 *
 *  Created on: Jul 14, 2010
 *      Author: psy
 */

#ifndef CSESSION_H_
#define CSESSION_H_

//#include "cgroup_types.h"

namespace asterisk
{
	using namespace std;
	using namespace boost;
	using namespace asterisk;
	typedef shared_ptr<class Csession> CsessionPtr;
	typedef map<int, CsessionPtr> CsessionMap;
}

#include "cgroup.h"

namespace asterisk
{
	using namespace std;
	using namespace boost;
	using namespace asterisk;

	//sessions: every synapse session has a corresponding session object here
	class Csession
	{
		private:
		int id;
		CgroupPtr groupPtr;

		public:
		Csession(int id);
		void setGroupPtr(CgroupPtr groupPtr);
		CgroupPtr getGroupPtr();
		string getStatus(string prefix);
	};

}
#endif /* CSESSION_H_ */

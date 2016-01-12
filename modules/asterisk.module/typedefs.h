#ifndef ASTERISK_TYPEDEF_H
#define ASTERISK_TYPEDEF_H

#include <boost/shared_ptr.hpp>
#include <map>
#include <string>

namespace asterisk
{
	using namespace boost;
	using namespace std;

	class CserverMan;

	typedef boost::shared_ptr<class Cserver> CserverPtr;
	typedef map<int, CserverPtr> CserverMap;

	typedef boost::shared_ptr<class Csession> CsessionPtr;
	typedef map<int, CsessionPtr> CsessionMap;

	typedef boost::shared_ptr<class Cchannel> CchannelPtr;
	typedef map<string, CchannelPtr> CchannelMap;

	typedef boost::shared_ptr<class Cdevice> CdevicePtr;
	typedef map<string, CdevicePtr> CdeviceMap;

	typedef boost::shared_ptr<class Cgroup> CgroupPtr;
	typedef map<string, CgroupPtr> CgroupMap;

	typedef long int TauthCookie;
}

#endif

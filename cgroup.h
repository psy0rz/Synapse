//
// C++ Interface: cgroup
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CGROUP_H
#define CGROUP_H
#include <boost/shared_ptr.hpp>
#include <string>
namespace synapse
{
using namespace boost;
using namespace std;

/**
	@author 
*/
class Cgroup{
public:
    Cgroup(const string & name);

    ~Cgroup();
    string getName();
private:
	string name;

};

typedef shared_ptr<Cgroup> CgroupPtr;
}
#endif

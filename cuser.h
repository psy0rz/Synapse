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













#ifndef CUSER_H
#define CUSER_H
#include "cgroup.h"
#include <string>
#include <list>
#include <boost/shared_ptr.hpp>

namespace synapse
{
  using namespace std;
  using namespace boost;

/**
	@author
*/
class Cuser{
public:
    Cuser(const string &name, const string &password);

    ~Cuser();
	  bool addMemberGroup(const CgroupPtr & group);
    bool isMemberGroup(const CgroupPtr & group);
    string getName();
    bool isPassword(const string & password);
    void print();

private:
	string name;
	string password;
	list<CgroupPtr > memberGroups;

};

typedef boost::shared_ptr<Cuser> CuserPtr;

}

#endif

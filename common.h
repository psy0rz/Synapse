#ifndef COMMON_H
#define COMMON_H

#include <string>

using namespace std;

struct eqstr
{
  bool operator()(string s1, string s2) const
  {
    return s1==s2;
  }
};


#endif

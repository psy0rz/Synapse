//
// C++ Implementation: clog
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "clog.h"
#include <iostream>
#include <boost/thread/mutex.hpp>

namespace synapse
{

boost::mutex logMutex;

}

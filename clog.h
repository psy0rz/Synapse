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













#ifndef CLOG_H
#define CLOG_H


#include <string>
#include <iostream>
#include <boost/thread/thread.hpp>
#include <sstream>

namespace synapse
{
extern boost::mutex logMutex;

//colors and ansi stuff (we dont want ncurses YET)
#define  TERM_BACK_UP "\033[1K\033[0G"
#define  TERM_NORMAL "\033[0m"
#define  TERM_WARN "\033[33;1m"
#define  TERM_BAD "\033[31;1m"
#define  TERM_SEND_MESSAGE "\033[32;1m"
#define  TERM_RECV_MESSAGE "\033[34;1m"
#define  TERM_BOLD "\033[1m"

//TODO: replace this with a better more efficient logging mechnism, altough its not trivial to make a thread-safe version of cout.

//thread-safe 'wrapper' around cout:
//we first buffer the output to a streamstream, since sometimes recursive calls happen from functions that are called while doing a << .
#define LOG(s) \
	{ \
		std::stringstream log_buff; \
		log_buff << s; \
		{ \
			boost::lock_guard<boost::mutex> lock(synapse::logMutex); \
			std::cout  << log_buff.rdbuf(); \
		} \
	}


//debug output with extra info:
#ifndef NDEBUG
#define DEB(s) LOG (boost::this_thread::get_id() << " " << "DEB: " << s << " @" << __FILE__ << ":" << __LINE__ << " in " << __PRETTY_FUNCTION__ << std::endl)
#else
#define DEB(s)
#endif

//normal info:
#define INFO(s) LOG(boost::this_thread::get_id() << " " << TERM_BOLD << "INFO: " << s << TERM_NORMAL << std::endl)

//errors:
#define ERROR(s) LOG(boost::this_thread::get_id() << " " << TERM_BAD << "ERROR: " << s << TERM_NORMAL << std::endl)

//warnings:
#define WARNING(s) LOG(boost::this_thread::get_id() << " " << TERM_WARN << "WARNING: " << s << TERM_NORMAL << std::endl)

//messages:
#define LOG_SEND(s) LOG(boost::this_thread::get_id() << " " << TERM_SEND_MESSAGE << s << TERM_NORMAL << std::endl)
#define LOG_RECV(s) LOG(boost::this_thread::get_id() << " " << TERM_RECV_MESSAGE << s << TERM_NORMAL << std::endl)


}

#endif

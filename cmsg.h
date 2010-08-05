//
// C++ Interface: cmsg
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CMSG_H
#define CMSG_H
#include <string>
#include <map>
#include <boost/any.hpp>
#include <boost/variant.hpp>
#include <boost/shared_ptr.hpp>

#include "cvar.h"
//#include "cmessageman.h"





/*
typical route of a cmsg object

1. Sendmessage is called with a shared_ptr to a Cmsg object:
   [Cmsg object] -smart_ptr-> Sendmessage() 
 	
2. This creates one or more Ccalls that point to it:
           [Ccall objects]  -shared_ptr->  [Cmsg object]

3. A operator() thread fetches a Iterator to a call object:
           operator() -iterator-> [Ccall object] --shared_ptr-> [Cmsg object]

4. The operator calls the apropriate handler with a reference to the message object:
     [Cmsg object] -reference-> soHander() 

5. The Ccall objects are deleted after the operators() are done:
    *poof* shared_ptr automaticly deletes [Cmsg object] after everyone is one. 
    INCLUDING the original sender of the message.



*/


/**
	@author 
*/

namespace synapse
{
	using namespace std;


	class Cmsg : public Cvar  {
	public:
		Cmsg();
		~Cmsg();
		int src;
		int dst;
		string event;

		//this function is only implemented in the .so object module
		//so if you use it in the core you will get linker errors :)
		void send(int cookie=0);
		void returnError(string description);
		void returnWarning(string description);
		bool returnIfOtherThan(char * keys, ...);

		//json stuff
		void toJson(string & jsonStr);
		void fromJson(string & jsonStr);

	};

	typedef shared_ptr<Cmsg> CmsgPtr;

}

typedef synapse::Cmsg Cmsg;

#endif

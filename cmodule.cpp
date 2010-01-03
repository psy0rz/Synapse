//
// C++ Implementation: cmodule
//
// Description: 
//
//
// Author:  <>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "cmodule.h"
#include "clog.h"
#include <dlfcn.h>
#include <boost/regex.hpp> 




Cmodule::Cmodule()
{
	maxThreads=1;
	currentThreads=0;
	broadcastMulti=false;
	soHandle=NULL;
	defaultSessionId=SESSION_DISABLED;
	soDefaultHandler=NULL;
	ready=false;
	
}


Cmodule::~Cmodule()
{
	//unload the module on destruction.
	//Destruction only happens when nobody is using the Cmodule object anymore, since we only
	//use shared_ptrs.
	if (soHandle)
	{
		DEB(name << " is unused - auto unloading");
		unload();
	}
}




/*!
    \fn Cmodule::startThread()
 */
bool Cmodule::startThread()
{
	if (currentThreads<maxThreads)
	{
		currentThreads++;
		return true;
	}
	return false;
}


/*!
    \fn Cmodule::endThread()
 */
void Cmodule::endThread()
{
	currentThreads--;
	assert(currentThreads>=0);
}


/*!
    \fn Cmodule::load(string name)
 */
bool Cmodule::load(string path)
{
	this->name=getName(path);
	this->path=path;
	INFO("Loading module " << name << " from: " << path );
	soHandle=dlopen(path.c_str(),RTLD_NOW);
	if (soHandle==NULL)
	{
		ERROR("Error loading module " << name << ": " << dlerror());
	}
	else
	{
		//check api version of the module
		FsoVersion version;
		version=(FsoVersion)resolve("synapseVersion");
		if (version!=NULL)
		{	
			if (version()!=SYNAPSE_API_VERSION)
			{
				ERROR("Module " << name << " has API version " << version() << " instead of : " << SYNAPSE_API_VERSION);
			}
			else 
			{
				//init function is MANDATORY
				soInit=(FsoInit)resolve("synapseInit");
				if (soInit!=NULL)
				{
					INFO("Module loading of " << name  << " complete.");
					return true;
				}
			}
		}

		DEB("Unloading because of previous error");
		dlclose(soHandle);
		soHandle=NULL;
	}
	return false;
}


/*!
    \fn Cmodule::unload()
 */
bool Cmodule::unload()
{
	if (soHandle!=NULL)
	{
		INFO("Unloading module " << name);

		FsoCleanup soCleanup;
		soCleanup=(FsoCleanup)resolve("synapseCleanup");
		if (soCleanup!=NULL)
		{
			DEB("Calling cleanup");
			soCleanup();
		}

		DEB("Calling dlclose on " << soHandle);
		if (dlclose(soHandle))
		{
			ERROR("Error unloading module " << name << ": " << dlerror());
		}
		DEB("Unload success");
	}
	return true;
}


/*!
    \fn Cmodule::getHandler(string name)
 */
FsoHandler Cmodule::getHandler(string eventName)
{
	ChandlerHashMap::iterator handlerI;
	handlerI=handlers.find(eventName);
	//not found?
	if (handlerI==handlers.end())
	{
		//return default handler
		return(soDefaultHandler);
	}
	else
	{
		return (handlerI->second);
	}
}

bool Cmodule::setHandler(const string & eventName, const string & functionName)
{
	FsoHandler soHandler;
	soHandler=(FsoHandler)resolve("synapse_"+functionName);
	if (soHandler!=NULL)
	{
		DEB(eventName << " -> " << functionName << "@" << name );
		handlers[eventName]=soHandler;

		return true;
	}

	return false;

}


/*!
    \fn Cmodule::resolve(const string * functionName)
 */
void * Cmodule::resolve(const string & functionName)
{
	if (soHandle==NULL)
	{
		ERROR("Cannot resolve function " << functionName << ". There is no module loaded! (yet?)");
		return NULL;
	}

	void * function;
	function=dlsym(soHandle, functionName.c_str());
	if (function==NULL)
	{
		ERROR("Could not find function " << functionName << " in module " << name << ": " << dlerror());
	}
	return (function);
}


/*!
    \fn Cmodule::isLoaded(string path)
 */
bool Cmodule::isLoaded(string path)
{
	return (dlopen(path.c_str(),RTLD_NOW|RTLD_NOLOAD)!=NULL);

}


/*!
    \fn Cmodule::getName(string path)
 */
string Cmodule::getName(string path)
{
	//calculate the name from a module path
	regex expression(".*/lib(.*).so$");
	cmatch what; 
	if(!regex_match(path.c_str(), what, expression))
	{
		ERROR("Cannot determine module name of path " << path);
		return (path);
	} 
	else
	{
		return (what[1]);
	}
	
}

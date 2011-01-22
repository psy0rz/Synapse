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


#include "chttpsession.h"

#include "../../csession.h"


ChttpSession::ChttpSession()
{
	netId=0;
	netInformed=false;
	lastTime=time(NULL);
	expired=false;
}

void ChttpSession::getStatus(Cvar & var)
{	
	var["expired"]=expired;
	var["lastTime"]=time(NULL)-lastTime;
	var["authCookie"]=mAuthCookie;
	var["queueLength"]=jsonQueue.length();
	var["netId"]=netId;
}

#include "cnet.h"
#include "synapse.h"

#include "json_spirit.h"

using namespace json_spirit;

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

// 	out.clear();
// 	out.event="core_ChangeModule";
// 	out["maxThreads"]=10;
// 	out.send();
// 
// 	out.clear();
// 	out.event="core_ChangeSession";
// 	out["maxThreads"]=10;
// 	out.send();
// 
// 	///tell the rest of the world we are ready for duty
// 	out.clear();
// 	out.event="core_Ready";
// 	out.send();

	out.clear();
	out.event="core_Register";
	out["handler"]="all";
	out.send();

	
}

/** Recursively converts a Cvar to a json_spirit Value.
*/
void Cvar2Value(Cvar &var,Value &value)
{
	switch(var.which())
	{
		case(CVAR_EMPTY):
			value=Value();
			break;
		case(CVAR_STRING):
			value=(string)var;
		case(CVAR_LONG_DOUBLE):
			value=(double)var;
			break;
		case(CVAR_MAP):
			//convert the CvarMap to a json_spirit Object with (String,Value) pairs 
			value=Object();
			for (Cvar::iterator varI=var.begin(); varI!=var.end(); varI++)
			{
				Value subValue;
				//recurse to convert the map-value of the CvarMap into a json_spirit Value:
				Cvar2Value(varI->second, subValue);
				
				//push the resulting Value onto the json_spirit Object
				value.get_obj().push_back(Pair(
					varI->first,
					subValue
				));
			}
			break;
		default:
			WARNING("Ignoring unknown variable type " << var.which());
			break;
	}
}


/** This handler is called for all events that:
 * -dont have a specific handler, 
 * -are send to broadcast or to a session we manage.
 */
SYNAPSE_HANDLER(all)
{

	//convert the message to a json-message
	Array jsonMsg;
	jsonMsg.push_back(msg.src);	
	jsonMsg.push_back(msg.dst);
	jsonMsg.push_back(msg.event);
	
	//convert the parameters, if any
	if (!msg.isEmpty())
	{
		Value jsonPars;
		Cvar2Value(msg,jsonPars);
	
		jsonMsg.push_back(jsonPars);
	}

	INFO("json: " << write( jsonMsg));
}




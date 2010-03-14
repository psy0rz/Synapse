#include "clog.h"
#include "cmsg.h"
#include "json_spirit.h"
#include <boost/asio.hpp>


using namespace json_spirit;
using namespace boost;


/** Recursively converts a json_spirit Value to a Cvar
*/
void Value2Cvar(Value &value,Cvar &var)
{
	switch(value.type())
	{
		case(null_type):
			var.clear();
			break;
		case(str_type):
			var=value.get_str();
			break;
		case(real_type):
			var=value.get_real();
			break;
		case(int_type):
			var=value.get_real();
			break;
		case(obj_type):
			//convert the Object(string,Value) pairs to a CvarMap 
			for (Object::iterator ObjectI=value.get_obj().begin(); ObjectI!=value.get_obj().end(); ObjectI++)
			{
				//recurse to convert the map-value of the CvarMap into a json_spirit Value:
				Value2Cvar(ObjectI->value_, var[ObjectI->name_]);
			}
			break;
		default:
			WARNING("Ignoring unknown json variable type " << value.type());
			break;
	}
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
			break;
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

/** Convert an incoming json message to a Cmsg
Format is:
	[6,0,"ami_Response_Success",{"ActionID":"Login","Message":"Authentication accepted","Response":"Success"}]

	srcid, dstid, eventname, parameterobject

*/
bool json2Cmsg(string &jsonStr, Cmsg & msg)
{
	//parse json input
//	std::istream readBufferIstream(&readBuffer);
	try
	{
		Value jsonMsg;
		//TODO:how safe is it actually to let json_spirit parse untrusted input? (regarding DoS, buffer overflows, etc)
		json_spirit::read(jsonStr, jsonMsg);
		msg.src=jsonMsg.get_array()[0].get_int();
		msg.dst=jsonMsg.get_array()[1].get_int();
		msg.event=jsonMsg.get_array()[2].get_str();
		Value2Cvar(jsonMsg.get_array()[3], msg);
		return true;
	}
	catch(...)
	{
		return false;
	}
}

/** Converts a message to a json string
 */
void Cmsg2json(Cmsg &msg, string & jsonStr)
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

	jsonStr=write(jsonMsg);
}



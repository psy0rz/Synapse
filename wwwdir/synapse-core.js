//disable logging if we dont have firebug.
//TODO:replace this with custom logging later perhaps.
if (typeof console == 'undefined')
{
	function Cconsole(){
	};
	Cconsole.prototype.debug=function() { };
	Cconsole.prototype.info=function() { };
	Cconsole.prototype.log=function() { };
	Cconsole.prototype.error=function() { };

	console=new Cconsole();
}

//user defined message handlers go in here:
synapse_handlers=new Array();

//synapse uses the authCookie inside the X-Synapse-Authcookie to distinguish different script-instances.
//No two script instances should use the same authCookie! otherwise you will miss certain messages.
synapse_authCookie=0;

//set this to true to stop polling:
synapse_stop=false;

//does a long poll to get events from the server
function synapse_receive()
{
	$.ajax({
		"dataType":		"json",
		"url":			'/synapse/longpoll',
		"error": 		synapse_handleError,
		"success":		synapse_handleMessages,
		"type":			"get",
		"contentType":	"application/json",
		"beforeSend":	function (XMLHttpRequest) {
			if (synapse_authCookie!=0) 
			{
				XMLHttpRequest.setRequestHeader("X-Synapse-Authcookie", synapse_authCookie); 
			}
			else
			{
				//request to clone the credentials of an older authcookie, so the user doesnt have to re-authenticate
				XMLHttpRequest.setRequestHeader("X-Synapse-Authcookie-Clone", $.readCookie('synapse_lastAuthCookie')); 
			}
		},
		"processData":	false,
		"cache":		false
	});
}

function escapeId(myid) { 
	//escape stuff to make it jquery compatible. 
	if (typeof myid != 'undefined')
		return '#' + myid.replace(/([,@\/.])/g,'\\$1');
	else
		return '#undefined';
}

function escapeClass(myclass) { 
	//escape stuff to make it jquery compatible. 
	if (typeof myclass != 'undefined')
		return '.' + myclass.replace(/([,@\/.])/g,'\\$1');
	else
		return '.undefined';
}

function synapse_register(event, handler)
{
	if (typeof synapse_handlers[event] == 'undefined')
	{
		synapse_handlers[event]=new Array();
	}
	synapse_handlers[event].push(handler);
}

function synapse_callHandlers(event)
{
	if (synapse_handlers[event])
	{
		for (var handlerI in synapse_handlers[event])
		{
			try	
			{
				synapse_handlers[event][handlerI](arguments[1], arguments[2], arguments[3], arguments[4])
			}
			catch(e)
			{
				console.error("Error while calling handler", e);
				if (event!="error")
				{
					synapse_callHandlers("error", "Exception error: "+e.message);
				}
			}

		}
		return true;		
	}	
	return false;
}

function synapse_handleError(request, status, e)
{
	errorTxt="Send/recieve error: " + request.responseText;
	console.error(errorTxt);
	synapse_callHandlers("error", errorTxt);
}

//handles long poll results, which contain incoming messages
function synapse_handleMessages(messages, status, XMLHttpRequest)
{
	//did we get a different authcookie back? 
	if (XMLHttpRequest.getResponseHeader("X-Synapse-Authcookie")!=synapse_authCookie)
	{
		//yes, store it for this session
		synapse_authCookie=XMLHttpRequest.getResponseHeader("X-Synapse-Authcookie");
		if (synapse_authCookie!=null)
		{
			//also store it in a real cookie so the user doesnt have to re-authenticate on refresh
			$.setCookie('synapse_lastAuthCookie', synapse_authCookie.toString(), {});
		}
	}

	if (messages==null)
	{
		synapse_callHandlers("error", "Connection error with server. Please reload page.");
		synapse_stop=true;
		return;
	}
	
	//traverse the received events
	for (var messageI in messages)
	{
		message=messages[messageI];
		console.debug("handling:",message);
	
		if (!synapse_callHandlers(message[2], message[0], message[1], message[2], message[3]))
		{
			//not found, try defaul handler
			synapse_callHandlers("default", message[0], message[1], message[2], message[3]);
		}
	}

	if (!synapse_stop)
	{
		synapse_receive();
	}
}

// function synapse_handleSent(result, status, XMLHttpRequest)
// {
// 	console.info("KJO",result,status);
// 	if (result==null)
// 	{
// 		synapse_callHandlers("error", "Connection error while sending message. Please reload page.");
// 	}
// }

function send(msg_dst, msg_event, msg)
{
	var jsonObj=[ 0, msg_dst, msg_event, msg ];
	console.debug("sending :", jsonObj);

	jsonStr=JSON.stringify(jsonObj);

	$.ajax({
		"dataType":		"text",
		"url":			'/synapse/send',
		"error": 		synapse_handleError,
/*		"success":		synapse_handleSent,*/
		"type":			"post",
		"contentType":	"application/json",
		"beforeSend":	function (XMLHttpRequest) { XMLHttpRequest.setRequestHeader("X-Synapse-Authcookie", synapse_authCookie); },
		"processData":	false,
		"cache":		false,
		"data":			jsonStr
	});
}

$(document).ready(function(){
	//need to start ansyncroniously, otherwise some browsers (android) keep loading:
	//(hmm this not always prevents the problem, we will figure out a better way later)
//	setTimeout("synapse_receive()",1000);
	synapse_receive();
});




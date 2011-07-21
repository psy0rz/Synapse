//Synapse javascript core functions (dont include directly)


//user defined message handlers go in here:
synapse_handlers=new Array();

//synapse uses the authCookie inside the X-Synapse-Authcookie to distinguish different script-instances.
//No two script instances should use the same authCookie! otherwise you will miss certain messages.
synapse_authCookie=0;

//set this to true to stop polling:
synapse_stop=false;

//enable message debugging (put synapse_debug somehwere in the url to enable!)
synapse_debug=false;

//que of messages that need to be sended.
synapse_sendQueue=""
	
//are we still sending something?
synapse_sending=false;

//does a long poll to get events from the server
function synapse_receive()
{
	$.ajax({
		"dataType":		"json",
		"url":			'/synapse/longpoll',
		"error": 		synapse_handleReceiveError,
		"success":		synapse_handleMessages,
		"type":			"get",
		"contentType":	"application/json",
		"beforeSend":	function (XMLHttpRequest) {
			if (synapse_authCookie) 
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
			//if we're debugging, let the browser/debugger handle exceptions
			if (synapse_debug)
				synapse_handlers[event][handlerI](arguments[1], arguments[2], arguments[3], arguments[4])
			else
			{
				//catch exceptions and generate a "nice" error
				try	
				{
					synapse_handlers[event][handlerI](arguments[1], arguments[2], arguments[3], arguments[4])
				}
				catch(e)
				{
					if (typeof e.stack != 'undefined')
						console.error("Error while calling handler:", e.stack);
					else
						console.error("Error while calling handler:", e);
						
					if (event!="error")
					{
						if (typeof e.stack != 'undefined')
							synapse_callHandlers("error", "Exception error: "+e.stack);
						else
							synapse_callHandlers("error", "Exception error: "+e.message);
					}
				}
			}
		}
		return true;		
	}	
	return false;
}

function synapse_handleReceiveError(request, status, e)
{
	errorTxt="Recieve error: " + request.responseText;
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
		var message=messages[messageI];

		if (synapse_debug)
			console.debug("handling:",JSON.stringify(message));
	
		if (!synapse_callHandlers(message[2], message[0], message[1], message[2], message[3]))
		{
			//not found, try defaul handler
			synapse_callHandlers("default", message[0], message[1], message[2], message[3]);
		}
	}

	if (!synapse_stop )
	{
		synapse_receive();
	}
	
	
}


function synapse_handleSendError(request, status, e)
{
	errorTxt="Send error: " + request.responseText;
	console.error(errorTxt);
	synapse_callHandlers("error", errorTxt);
	synapse_sending=false;
	sendQueue();
}

function synapse_handleSend(request, status, e)
{
	synapse_sending=false;
	sendQueue();
}


function sendQueue()
{
	//no more to send?
	if (synapse_sendQueue=="")
		return;	
	
	//still sending?
	if (synapse_sending)
		return;

	
	$.ajax({
		"dataType":		"text",
		"url":			'/synapse/send',
		"success":		synapse_handleSend,
		"error": 		synapse_handleSendError,
		"type":			"post",
		"contentType":	"application/json",
		"beforeSend":	function (XMLHttpRequest) { 
			if ( synapse_authCookie!=0) 
			{
				XMLHttpRequest.setRequestHeader("X-Synapse-Authcookie", synapse_authCookie); 
			}
			else
			{
				//neccesary when sending DelSession on the window.unload event. 
				//the browser uninitalizes all the variables.
				XMLHttpRequest.setRequestHeader("X-Synapse-Authcookie", $.readCookie('synapse_lastAuthCookie')); 
			}
		},
		"processData":	false,
		"cache":		false,
		"data":			synapse_sendQueue
	});

	synapse_sending=true;
	synapse_sendQueue="";
}


function send(msg_dst, msg_event, msg)
{
	var jsonObj=[ 0, msg_dst, msg_event, msg ];

	
	jsonStr=JSON.stringify(jsonObj);

	if (synapse_debug)
		console.debug("sending :", jsonStr);

	if (synapse_sendQueue=="")
	{
		synapse_sendQueue+=jsonStr;
	}
	else
	{
		synapse_sendQueue+="\n"+jsonStr;
	}
	
	sendQueue();
}

$(document).ready(function(){
	
	//need to start ansyncroniously, otherwise some browsers (android) keep loading:
	//(hmm this not always prevents the problem, we will figure out a better way later)
	setTimeout("synapse_receive()",100);
	//synapse_receive();
	
	synapse_debug=(window.location.search.indexOf('synapse_debug') != -1);
	
	//whenver the user leaves the page, try to leave the session
	//(if it fails its no problem: the session will eventually timeout in http_json)

	//inoffical propiertary way, but earlier so message sending still works
	//DONT, doesnt allow refresh while auto-loggin in again!
//	window.onbeforeunload=function()
//	{
//		send(0,"core_DelSession");
//	}

	//official way, but too late for browsers like chromium
	//$(window).unload(function()
	//{
	//send(0,"core_DelSession");
	//});

	

});




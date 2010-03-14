//disable logging if we dont have firebug.
//we replace this with custom logging later perhaps.
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

//synapse uses the authCookie inside the X-Synapse-Authcookie to distinguish different script-instances
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
		"beforeSend":	function (XMLHttpRequest) { XMLHttpRequest.setRequestHeader("X-Synapse-Authcookie", synapse_authCookie); },
		"processData":	false,
		"cache":		false
	});
}


function synapse_handleError(request, status, e)
{
	errorTxt="Error while processing request: " + request.responseText;
	console.error(errorTxt);
	if (synapse_handlers["error"])
	{
		synapse_handlers["error"](errorTxt);
	}
}

//handles long poll results, which contain incoming messages
function synapse_handleMessages(messages, status, XMLHttpRequest)
{
	//did we get an authcookie? 
	if (XMLHttpRequest.getResponseHeader("X-Synapse-Authcookie"))
	{
		synapse_authCookie=XMLHttpRequest.getResponseHeader("X-Synapse-Authcookie");
	}

	if (messages==null)
	{
		if (synapse_handlers["error"])
		{
			synapse_handlers["error"]("Got null json reply, stopping.");
			synapse_stop=true;
		}
		return;
	}
	
	//traverse the received events
	for (var messageI in messages)
	{
		message=messages[messageI];
		console.debug("handling:",message);
	
		try
		{
			//handler exists?
			if (synapse_handlers[message[2]])
			{
				//call it
				synapse_handlers[message[2]](message[0], message[1], message[2], message[3]);
			}
			//default handler exists?
			else
			{
				if (synapse_handlers["default"])
				{
					//call it
					synapse_handlers["default"](message[0], message[1], message[2], message[3]);
				}
			}
		}
		catch(e)
		{
			if (synapse_handlers["error"])
			{
				//call it
				synapse_handlers["error"](e.description);
			}
		}
	}

	if (!synapse_stop)
	{
		synapse_receive();
	}
}


function synapse_register(event, handler)
{
	synapse_handlers[event]=handler;
}

function send(msg_dst, msg_event, msg)
{
	var jsonObj=[ 0, msg_dst, msg_event, msg ];
	console.debug("sending :", jsonObj);

	jsonStr=JSON.stringify(jsonObj);

	$.ajax({
		"dataType":		"text",
		"url":			'/synapse/send',
		"error": 		synapse_handleError,
		"type":			"post",
		"contentType":	"application/json",
		"beforeSend":	function (XMLHttpRequest) { XMLHttpRequest.setRequestHeader("X-Synapse-Authcookie", synapse_authCookie); },
		"processData":	false,
		"error": 		synapse_handleError,
		"cache":		false,
		"data":			jsonStr
	});
}

$(document).ready(function(){
	//need to start ansyncroniously, otherwise some browsers (android) keep loading:
	//(hmm this not always prevents the problem, we will figure out a better way later)
	setTimeout("synapse_receive()",1000);
});




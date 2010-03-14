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

//we count the number of outstanding xmlhttprequests: we make sure at least on is doing a longpoll
synapse_requestCount=0;

//synapse uses the authCookie inside the X-Synapse-Authcookie to distinguish different script-instances
synapse_authCookie=0;

//set this to true to stop polling:
//you can still SEND events, which will result in one poll, but after that it stops
synapse_stop=false;


function synapse_doRequest(jsonStr)
{

	//only send a request if we have data to send, or if there are no more outstanding longpolls
	if (typeof jsonStr!='undefined' || ( synapse_stop==false && synapse_requestCount==0))
	{
		synapse_requestCount++;

		if (typeof jsonStr=='undefined')
		{
			type="get";
		}
		else
		{	
			type="post";
		}

		$.ajax({
			"dataType":		"json",
			"url":			'/synapse/longpoll',
			"error": 		synapse_handleError,
			"success":		synapse_handleMessages,
			"type":			type,
			"contentType":	"application/json",
			"beforeSend":	function (XMLHttpRequest) { XMLHttpRequest.setRequestHeader("X-Synapse-Authcookie", synapse_authCookie); },
			"processData":	false,
			"cache":		false,
			"data":			jsonStr
		});
	}
}


function synapse_handleError(request, status)
{
	synapse_requestCount--;
	if (synapse_handlers["error"])
	{
		synapse_handlers["error"]("Error while processing ajax request '" + request + "' status=" + status);
	}
}


function synapse_handleMessages(messages, status, XMLHttpRequest)
{
	//did we get an authcookie? 
	if (XMLHttpRequest.getResponseHeader("X-Synapse-Authcookie"))
	{
		synapse_authCookie=XMLHttpRequest.getResponseHeader("X-Synapse-Authcookie");
	}

	synapse_requestCount--;
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

	synapse_doRequest();
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
	synapse_doRequest(jsonStr);
}

 $(document).ready(function(){
	synapse_doRequest();
});




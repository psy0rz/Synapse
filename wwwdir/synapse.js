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

synapse_handlers=new Array();

synapse_requestCount=0;


var requests=0;
var answers=0;

function doRequest(jsonStr)
{

	//only send a request if we have data to send, or if there are no more outstanding longpolls
	if (typeof jsonStr!='undefined' || synapse_requestCount==0)
	{
		requests++;
		console.debug("requests=",requests);

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

var stop=false;

function synapse_handleMessages(messages, status, requestObject)
{
	if (stop)
	{
		console.debug("stopping, dropped message");
		return;
	}

	answers++;
	console.debug("answers=",answers);

	synapse_requestCount--;
	if (messages==null)
	{
		if (synapse_handlers["error"])
		{
			synapse_handlers["error"]("Got null json reply, stopping.");
			stop=true;
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

	doRequest();
}

function send(msg_dst, msg_event, msg)
{
/*	json=new Array();
	json[0]=0;
	json[1]=msg_dst;
	json[2]=msg_event;
	json[3]=msg;*/
	
	jsonStr=JSON.stringify([ 0, msg_dst, msg_event, msg ]);

	doRequest(jsonStr);
}

function synapse_register(event, handler)
{
	synapse_handlers[event]=handler;
}


 $(document).ready(function(){
// 
// 		$.ajax({
// 			"dataType":		"json",
// 			"url":			'/synapse/longpoll',
// 			"type":			"get",
// 			"contentType":	"application/json",
// 			"processData":	false,
// 			"cache":		false,
// 			"data":			[ ] 
// 		});

//window.setInterval('doRequest()');
	doRequest();
});




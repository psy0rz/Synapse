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

console.debug("dsa");

var synapse_handlers=new Array();


// function send(msg_dst, msg_event, msg)
// {
// /*	$.ajax({
// 
// 		dataType: "json",
// 		url: '/synapse/send',
// 		success: synapse_handleMessages,
// 		error: synapse_handleError
// 	});*/
// }

function synapse_register(event, handler)
{
	synapse_handlers[event]=handler;
}

function synapse_handleError(request, status)
{
	if (synapse_handlers["error"])
	{
		synapse_handlers["error"]("Error while processing ajax request: " + status);
	}
}

function synapse_handleMessages(messages, status)
{
	if (messages==null)
	{
		if (synapse_handlers["error"])
		{
			synapse_handlers["error"]("Got null json reply, stopping.");
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

	$.ajax({

		dataType: "json",
		url: '/synapse/longpoll',
		success: synapse_handleMessages,
		error: synapse_handleError
	});
}



$(document).ready(function(){
	//place code inside this block:

	synapse_handleMessages(Array());
});




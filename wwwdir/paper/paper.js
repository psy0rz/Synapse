// (C)edwin@datux.nl - released under GPL 
// part of Synapse

//Paper core drawing functionality. Requires a bunch of dom object to function (see edit.html)

function Cpoint(x,y)
{
	this.x=x;
	this.y=y;

	this.copy=function(point)
	{
		this.x=point.x;
		this.y=point.y;
	}
	
}

var mouseInterval=40; //max update inteval in mS (can be higer if mouseMax is exceeded)
var mouseMax=10; //max mouse pixel offset before sending (regardless of update interval)

var mousePoint=new Cpoint(); //current mouse position
var mousePointStart=new Cpoint(); //mouse position at start of current operation

//mouse info sending to server:
var mouseSendPoint=new Cpoint(); //last mousepoint sent to server
var mouseSendTime=0; //last time the mousePosition was anaylsed by updateGameField

var mouseMode="";		
var mouseTarget="";
var mouseTargetHigh="";
var mouseHighlight=0;

//temporary local objects counter
var tempCount=0;

var currentObjectId=0;
var paperModId=0;

	
//incremental draw stuff
var clientId=0;
var clients=new Array(); 
var ourId=0;

var drawing;
var drawingInsert;

var aspectRatio=1.7777;

var loading=true;

//chat stuff
var chatLastClientName="";
var chatTypeHere="Type hier je bericht...";

//current rights
var authorized;
		

//filter dangerous stuff from user input.
//NOTE: do this when you receive the data back from the server, not when sending to it!
function filterInput(s,max)
{
	if (s==null)
		return;
	ret=s.replace(/</g,"&lt;").replace(/>/g,"&gt;");

	if (max!=null && ret.length>max)
		ret=ret.substr(0,max);

	return(ret);
}

//calculate browser coordinates to server coordinates
function toServer(x,y)
{
	
	//return ( { "x":x, "y":y } );
	return ({
		"x":Math.round((x * 10000 * aspectRatio) / $("#drawing").width()),
		"y":Math.round((y * 10000) / $("#drawing").height())
	});
}



function paperLink(id,text)
{
	return ("<a class='paperLink' href='paper.html?id="+id+"'>"+text+"</a>");
}



//highlight an object
function highlight(id)
{
	//add highlite, if we can find the element:
	var element=document.getElementById(id);
	if (element!=null)
	{
		element.setAttribute('opacity','0.75');
		element.setAttribute('stroke-opacity','0.75');
		element.setAttribute('fill-opacity','0.75');
	}
}

function unhighlight(id)
{
	//remove highlite, if we can find the element:
	var element=document.getElementById(id);
	if (element!=null)
	{
		element.removeAttribute('opacity');
		element.removeAttribute('stroke-opacity');
		element.removeAttribute('fill-opacity');
	}
}

function chatPrintOnline()
{
	var online=$('.chatClient').length-1;
	if (online==0)
		$("#chatBar").html("<div class='chatOnline'> Chatbox</div>");
	else if (online==1)
		$("#chatBar").html("<div class='chatOnline'>1 vriend online</div>");
	else
		$("#chatBar").html("<div class='chatOnline'>"+online+" vrienden online</div>");
}

//determine the id of the paper the user wants to edit
function getUrlId()
{

	var file=$.url(document.location).segment(1);
	if (file==parseInt(file))
		return(file);
	else
		return($("head").attr("paperId"));
}

function updateAuthorisation(newRights)
{
	authorized=newRights;
	//automaticly add/remove classes
	$.each(authorized, function(authType, authOn)
	{
		if (authOn)
		{
			$(".auth_"+authType).removeClass("auth_"+authType+"_disabled");
			$(".auth_"+authType).addClass("auth_"+authType+"_enabled");
		}
		else
		{
			$(".auth_"+authType).addClass("auth_"+authType+"_disabled");
			$(".auth_"+authType).removeClass("auth_"+authType+"_enabled");
		}
	});
}

//reset and prepare for a complete reload of all data
function reset()
{
	//remove all child nodes from drawing
	while(drawing.hasChildNodes())
	{
		drawing.removeChild(drawing.lastChild);
	}

	//add a node that indicates where drawing elements should be inserted before
	drawingInsert = document.createElementNS(svgns, 'g');
	drawing.appendChild(drawingInsert);

	
	$('#clientList').empty();
	$('#chatList').empty();
	$("#chatInput").val("");
	
	
	//select default tools
	$(".tool").removeClass('selected');
	$(".defaultTool").addClass('selected');
	$(".defaultSetting").show();
	

}

//create/get cursor svg object for specified client
function getCursor(clientId)
{
	var cursor=document.getElementById('cursor'+clientId);
	if (cursor==null)
	{
		//create a group for this client's cursor
		cursor = document.createElementNS(svgns, 'g');
		cursor.setAttribute('id', 'cursor'+clientId);
		cursor.setAttribute('stroke', '#000000');
		cursor.setAttribute('stroke-width', '10');
		drawing.appendChild(cursor);
	
		//add drawing to the cursor group
		var e = document.createElementNS(svgns, 'line');
		e.setAttribute('x1', '-200');
		e.setAttribute('x2', '+200');
		e.setAttribute('y1', '0');
		e.setAttribute('y2', '0');
		cursor.appendChild(e);					
	
		var e = document.createElementNS(svgns, 'line');
		e.setAttribute('x1', '0');
		e.setAttribute('x2', '0');
		e.setAttribute('y1', '-200');
		e.setAttribute('y2', '+200');
		cursor.appendChild(e);					
	
		//create text element
		var e = document.createElementNS(svgns, 'text');
		e.setAttribute('id', 'cursorName'+clientId);
		e.setAttribute('x', '0');
		e.setAttribute('y', '300');
		e.setAttribute('font-size','200');
		e.setAttribute('font-weight','bold');
		e.setAttribute('stroke','#000000');
		e.setAttribute('fill','#000000');
		cursor.appendChild(e);					
	
		//add the actual text
		var textNode = document.createTextNode("(unknown)", true);
		e.appendChild(textNode);
		
	}
	return (cursor);
}


/*** Drawing protocol

Drawing stuff is done with paper_clientDraw.
The message is translated to a paper_serverDraw and forwarded to all clients.
The message is normally not echoed back, unless requested

### Common parameters
src:		added by server. session id that send the message

### Drawing commands and parameters:
cmd:		command to do:
				update: create/modify element 
				delete: remove element
				refresh: ask the server to resend an update cmd with all info about the specified element, to this client. (message wont be relayed to clients) 
				reload: indicates the start of a reload of ALL data (drawing, chat, cursors etc) (message wont be relayed to clients)
				ready: indicates refresh of all data is ready (message can only be send by server)
			
Only for updates with new (not yet existing) objects:
element:		svg element type (e.g. circle,rect,polyline)

update parameters:
add:    	svg attributes to append data to (used for polylines)
set:		svg attributes to set/replace


update/delete parameters:		
id:			element id for update/delete. 
			if set to 'new' with an update, the server will create a new element and replace it with its id.
			if not set by with an update/refresh, the id is replaced by id used in the last 'update' for this client. 

beforeId:   set by server to the id of the element this element should be inserted before. this is important to keep the order of all the elements correct.
normally only set when requesting a refresh.

#### Cursor indication
cursor:     Cursor and name information: x,y 
			The server keeps a list of cursors and sends them when a client joins.

### Chat
chat:		Chat text: text, clientName. 
			The server keeps the history and sends it if a client joins.
		
'
 */


function draw(msg)
{
	var suspendID = drawing.suspendRedraw(250);
	 
	//create/update object
	if (msg["cmd"]=="update")
	{
		if (msg["id"]!=null)
		{
			var element=document.getElementById(msg["id"]);
			//element not found? 
			if (element==null)
			{
				//create new element
				var element = document.createElementNS(svgns, msg["element"]);
				element.setAttribute('id', msg["id"]);

				//insert before existing or just append?
				var beforeElement=null;
				if (msg["beforeId"]!=null)
					beforeElement=document.getElementById(msg["beforeId"]);

				//text element? append a special textnode to hold the actual text
				if (msg["element"]=="text")
				{
					var textNode = document.createTextNode("", true);
					element.appendChild(textNode);
				}
				
				//NOTE: svgweb is buggy and doesnt support insertbefore with a null element
				if (beforeElement==null)
					drawing.insertBefore(element, drawingInsert);							
				else
					drawing.insertBefore(element, beforeElement);

				//temporary object? highlight it
				if (msg["id"] != parseInt(msg["id"]))
				{
					highlight(msg["id"]);
				}
			}
			
			//we need to add/set attributes?
			if (msg["add"] || msg["set"])
			{
				for (var attrName in msg["set"])
				{	
					//special text hack 'attribute'?
					if (attrName=='text')
					{
						element.childNodes[0].data=filterInput(msg["set"][attrName],100);
					}
					//xlink namespace attribute
					else if (attrName.substr(0,5)=="xlink")
					{
						element.setAttributeNS(xlinkns, attrName, msg["set"][attrName]);
					}							
					//somehow it seems we need to ignore these (svgweb or in general?)
					else if (attrName.substr(0,5)=="xmlns")
					{
						continue;
					}	
					//normal attribute
					else
					{
						element.setAttribute(attrName, msg["set"][attrName]);
					}
				}							

				for (var attrName in msg["add"])
				{
					if (element.getAttribute(attrName)!=null)
					{
						element.setAttribute(attrName, element.getAttribute(attrName)+msg["add"][attrName]);
					}
					else
						element.setAttribute(attrName, msg["add"][attrName]);
				}
			}
		}
		
		//remove any temporary objects
		if (msg["tempId"]!=null)
		{
			var element=document.getElementById(msg["tempId"]);
			if (element!=null)
			{
				drawing.removeChild(element);
			}
		}		

	}
	//delete object
	else if (msg["cmd"]=="delete")
	{
		//console.log("DEL"+msg["id"]);
		var element=document.getElementById(msg["id"]);
		if (element!=null)
		{
			drawing.removeChild(element);
		}	
	}
	//start of complete refresh 
	else if (msg["cmd"]=="reload")
	{
		$("#loading").show();
		loading=true;		
		reset();
	}
	//ready with complete refresh
	else if (msg["cmd"]=="ready")
	{
		//NOTE: this also works around a weird chromuim refresh bug.
		$("#loading").hide();
		loading=false;		
	}			

	//update cursor?
	if (msg["cursor"]!=null && msg["src"]!=ourId)
	{
		//get/create cursor:
		var cursor=getCursor(msg["src"]);

		//update cursor position?
		if (msg["cursor"]["x"]!=null)
		{
			cursor.setAttribute('transform', 'translate('+msg["cursor"]["x"]+','+msg["cursor"]["y"]+')');
		}
	}



	//received chat text?
	if (msg["chat"] != null)
	{
		var clientName=filterInput(msg["chat"]["clientName"],15);
		var text=filterInput(msg["chat"]["text"]);
						
		if (clientName!=chatLastClientName)
		{
			//add client name:
			$('#chatList').append("<div class='chatName'>"+clientName+":</div>");
			chatLastClientName=clientName;					
		}

		$('#chatList').append("<div class='chatText'>"+text+"</div>");

		//hidden?
		if ($("#chatBox").css("display")=="none")
		{
			if (!loading)
			{
				var html="<b>"+clientName+"</b>: "+text;
				if (html.length>30)
					html=html.substr(0,30)+"...";
				$("#chatBar").html(html);
				$("#chatBar").effect("highlight","{}",1000);
			}
		}
		else
		{
			//scroll to end
			var objDiv = document.getElementById("chatList");
			objDiv.scrollTop = objDiv.scrollHeight;
		}
				
	}

	drawing.unsuspendRedraw(suspendID);
}

//someone left, remove their cursor and stuff
synapse_register("object_Left",function(msg_src, msg_dst, msg_event, msg)
{
	var cursor=document.getElementById('cursor'+msg["clientId"]);
	if (cursor!=null)
	{
		drawing.removeChild(cursor);
	}

	$('#chatClient'+msg["clientId"]).remove();
	chatPrintOnline();

});


//received a drawing command from the server
synapse_register("paper_ServerDraw",function(msg_src, msg_dst, msg_event, msg)
{
	draw(msg);
});


//send drawing msg to server 
//note: you need to specifiy server coordinates instead of browser coordinates here.		
function sendDraw(msg)
{
	//only if we're joined to something
	if (!currentObjectId)
		return;
	
	//send it to server
	send(paperModId,"paper_ClientDraw", msg);

	//the server doesnt echo our stuff back, so we can handle it locally without lag
	//call the draw function and emulate the server by adding some stuff
	msg["src"]=ourId;
	if (msg["id"]==null || msg["id"]=="new")
		msg["id"]="temp"+tempCount;
	draw(msg);
}


//user moves mouse		
function mouseMove(force)
{
	
	//mouse throttling function, to prevent too many updates per second
	//no change?
	if (force!=true && mousePoint.x==mouseSendPoint.x && mousePoint.y==mouseSendPoint.y)
		return;

	currentTime=new Date().getTime();

	//send updates at a max fps, or more when the mouse moves fast
	if (	force==true ||
			(currentTime-mouseSendTime>=mouseInterval) || 
			Math.abs(mousePoint.x-mouseSendPoint.x)> mouseMax ||
			Math.abs(mousePoint.y-mouseSendPoint.y)> mouseMax 
	)	
	{ 
		//highlighting target changed?
		if (mouseHighlight && mouseTarget!=mouseTargetHigh)
		{
			//remove old highlite (if any)
			unhighlight(mouseTargetHigh);
			//valid new target?
			if (mouseTarget == parseInt(mouseTarget))
			{
				highlight(mouseTarget);
			}
			mouseTargetHigh=mouseTarget;
		}

		
		var c=toServer(mousePoint.x,mousePoint.y);
		var msg={};

		//cursor authorized?
		if (authorized.cursor)
			msg['cursor']=c;
		else 
			return; //no? then we may not do anything
		
		if (mouseMode=="polyline")
		{
			msg['cmd']='update';
			msg['add']={
				'points': ' '+c.x+','+c.y
			}
		}
		else if (mouseMode=="line")
		{
			msg['cmd']='update';
			msg['set']={
				'x2': c.x,
				'y2': c.y,
			}
		}
		else if (mouseMode=="rect" || mouseMode=="image")
		{
			var cStart=toServer(mousePointStart.x,mousePointStart.y);
			var w=c.x-cStart.x;
			var h=c.y-cStart.y;
			if (w<100)
				w=100;
			if (h<100)
				h=100;
			
			msg['cmd']='update';
			msg['set']={
				'width': w,
				'height': h					
			}
		}
		else if (mouseMode=="circle")
		{
			var cStart=toServer(mousePointStart.x,mousePointStart.y);
			var rx=Math.abs(c.x-cStart.x);
			var ry=Math.abs(c.y-cStart.y);
			//var r=Math.sqrt(Math.pow(xoffset,2)+Math.pow(yoffset,2));
			
			if (rx<100)
				rx=100;
			if (ry<100)
				ry=100;
			
			msg['cmd']='update';
			msg['set']={
				'rx': rx,
				'ry': ry
			}
		}
		else if (mouseMode=="text")
		{
			var cStart=toServer(mousePointStart.x,mousePointStart.y);
			var r=Math.round(200+Math.sqrt(Math.pow(c.x-cStart.x,2)+Math.pow(c.y-cStart.y,2))/5);
			msg['cmd']='update';
			msg['set']={
				'font-size': r
			}
		}
		else if (mouseMode=="delete")
		{ 
			//only allow numeric id's to be deleted. otherwise you could delete cursors and stuff as well :P
			if (mouseTarget == parseInt(mouseTarget))
			{
				msg['cmd']='delete';
				msg['id']=mouseTarget;
			}				
		}
		else if (mouseMode=="move")
		{
			var cStart=toServer(mousePointStart.x,mousePointStart.y);
			var dx=(c.x-cStart.x);
			var dy=(c.y-cStart.y);
			
			msg['cmd']='update';
			msg['id']=mouseTargetStart;
			msg['set']={
				'transform':'translate('+dx+','+dy+')'
			}
		}

		sendDraw(msg);

		mouseSendPoint.x=mousePoint.x;
		mouseSendPoint.y=mousePoint.y;
		mouseSendTime=currentTime;
	}
}	

//user starts a mouse operation (clicks somewhere)
function mouseStart(m)
{
	if (!authorized.change)
		return;
	
	if (mousePoint.x==null || mousePoint.y==null)
		return;
	tempCount++;
	mouseMode=$('#modes .selected').attr("value");
	mouseTargetStart=mouseTarget;
	mousePointStart.x=mousePoint.x;
	mousePointStart.y=mousePoint.y;

	//determine attributes
	var fill='none';
	var strokeWidth=$('#width').slider("option","value")*25;
	if ($('#modes .selected').attr("fill")!=null)
	{
		fill=$('#colors .selected').attr("value");
		strokeWidth=1;
	}
	var stroke=$('#colors .selected').attr("value"); 
	var strokeDashArray=$('#styles .selected').attr("value");
	
	//do the action:
	if (mouseMode=="polyline")
	{
		sendDraw({
			'cmd'		:'update',
			'id'		:'new',
			'element'	:'polyline',
			'set'		: {
				'fill'				: fill,
				'stroke'			: stroke,
				'stroke-width'		: strokeWidth,
			}
		});
	}
	else if (mouseMode=="rect")
	{
		var c=toServer(mousePoint.x,mousePoint.y);

		sendDraw({
			'cmd'		:'update',
			'id'		:'new',
			'element'	:'rect',
			'set'		: {
				'fill'				: fill,
				'stroke'			: stroke,
				'stroke-width'		: strokeWidth,
				'x'					: c.x, 
				'y'					: c.y
			}
		});
	}
	else if (mouseMode=="line")
	{
		var c=toServer(mousePoint.x,mousePoint.y);

		sendDraw({
			'cmd'		:'update',
			'id'		:'new',
			'element'	:'line',
			'set'		: {
				'fill'				: fill,
				'stroke'			: stroke,
				'stroke-width'		: strokeWidth,
				'x1'				: c.x, 
				'y1'				: c.y
			}
		});
	}
	else if (mouseMode=="circle")
	{
		var c=toServer(mousePoint.x,mousePoint.y);

		sendDraw({
			'cmd'		:'update',
			'id'		:'new',
			'element'	:'ellipse',
			'set'		: {
				'fill'				: fill,
				'stroke'			: stroke,
				'stroke-width'		: strokeWidth,
				'cx'				: c.x, 
				'cy'				: c.y
			}
		});
	}
	else if (mouseMode=="text")
	{
		if ($("#textInput").val()!="")
		{
			var c=toServer(mousePoint.x,mousePoint.y);
			sendDraw({
				'cmd'		:'update',
				'id'		:'new',
				'element'	:'text',
				'set'		: {
					'fill'				: fill,
					'x'					: c.x, 
					'y'					: c.y,
					'stroke'			: stroke,
					'text'				: $("#textInput").val(),
					'font-family'		: "arial",
					'font-size'			: 200,
					'font-weight'		: 'bold'
				}
			});
		}
		else
		{
			mouseMode="";
		}
	}
	else if (mouseMode=="image")
	{
		if ($("#imageInput").val()!="")
		{
			var c=toServer(mousePoint.x,mousePoint.y);
			sendDraw({
				'cmd'		:'update',
				'id'		:'new',
				'element'	:'image',
				'set'		: {
					'preserveAspectRatio'	: 'none',
					'x'						: c.x, 
					'y'						: c.y,
					'xlink:href'			: $("#imageInput").val(),
					'xlink:title'			: $("#imageInput").val()
				}
			});
		}
		else
		{
			mouseMode="";
		}
	}

	//force a mouse move
	mouseMove(true);
}

//user ends operation (leaves or mouse up)
function mouseEnd(m)
{
	if (mouseMode!="")
	{
		mouseMode="";
		sendDraw({
			'cmd'		:'refresh',
			'tempId'	:'temp'+tempCount
		});				
	};
	unhighlight(mouseTargetHigh);
	mouseTargetHigh="";
}


    
synapse_register("module_SessionStart",function(msg_src, msg_dst, msg_event, msg)
{
	ourId=msg_dst;

	/// figure out client name
	var clientName=$.readCookie('clientName', { path:"/" } );
	if (!clientName)
	{
		//make up a "random" guest name
		clientName="User "+msg_dst;
	}
	
	//set the name input field
	$("#chatClientName").val(clientName);

	//width slider
	$("#width").slider(
	{ 
		min:1, 
		value:3,
		max:10,
		slide: function(event, ui) {
			$("#widthText").html(ui.value);
		}
	});

	//delete the session just before unloading 
	window.onbeforeunload=function()
	{
		send(0,"core_DelSession");
	}
	
	
	/// JAVA SCRIPT EVENT HANDLERS
	
	//window resize
	$(window).resize(function()
	{
		if ($('#drawingSize').height()*aspectRatio <= $('#drawingSize').width())
		{
			$('#drawing').height($('#drawingSize').height());
			$('#drawing').width($('#drawingSize').height()*aspectRatio);
		}
		else
		{
			$('#drawing').width($('#drawingSize').width());
			$('#drawing').height($('#drawingSize').width()/aspectRatio);
		}
	});
	$(window).resize();
	

	//user changes name
	$("#chatClientName").keyup(function(m)
	{
		$.setCookie('clientName', $("#chatClientName").val(), { duration:365, path:"/" });
		send(paperModId,"paper_ChangeInfo", { 
			name: $("#chatClientName").val()
		});			
	});

	//user presses enter in chatInput:
	$("#chatInput").keydown(function(m)
	{
		if (m.keyCode==13)
		{
			sendDraw( {
				'chat':{
					'clientName'	:$("#chatClientName").val(),
					'text'			:$("#chatInput").val()
				}
			});
		}					
	});

	$("#chatInput").keyup(function(m)
	{
		if (m.keyCode==13)
			$("#chatInput").val("");						
	});

	$("#chatInput").focusin(function(m)
	{

		$("#chatInput").removeClass("chatTypeHere");
		if ($("#chatInput").val()==chatTypeHere)
			$("#chatInput").val("");
	});
			
	$("#chatInput").focusout(function(m)
	{
		if ($("#chatInput").val()=="")
		{
			$("#chatInput").addClass("chatTypeHere");
			$("#chatInput").val(chatTypeHere);
		}
	});


	//once a autofocus field gets focus, it will be rememberd, and focus will be switched back automaticly
	$(".autoFocus").focusin(function(m)
	{
		$(".focus").removeClass("focus");
		$(this).addClass("focus");
	});

	$(window).click(function(m)
	{
		$(".focus").focus();
	});
	
	
	
	//hide/show chatbox
	$("#chatBar").click(function()
	{
		//show
		if ($("#chatBox").css("display")=="none")
		{
			$(this).addClass("chatBarOpen");
			$("#chatBox").show();

			//scroll to end
			var objDiv = document.getElementById("chatList");
			objDiv.scrollTop = objDiv.scrollHeight;

			$("#chatInput").focus();
			
			$("#drawingSize").css("right",$("#chatBox").width());
			$(window).resize();
			chatPrintOnline();
		}
		//hide
		else
		{
			$(this).removeClass("chatBarOpen");
			$("#chatBox").hide();
			$("#drawingSize").css("right","0");
			$(window).resize();
		}
	});

	//hide/show a menu
	$(".menuBar").click(function()
	{
		$(this).children(".menu").toggle();
		$(this).toggleClass("menuBarOpen");
	});

			
	
	//a tool is clicked
	$(".tool").click(function(m)
	{
		//unselect all tools in that group
		$(this).parent().children(".tool").removeClass('selected');
		//select this one
		$(this).addClass('selected');

		//switch highlite mode on?
		if ($(this).attr('changeHigh')=='1')
			mouseHighlight=1;

		//switch highlite mode off?
		if ($(this).attr('changeHigh')=='0')
			mouseHighlight=0;

		//show/hide tool specific settings
		if ($(this).attr('changeShow')!=null)
		{
			$(".setting").hide();
			$($(this).attr('changeShow')).show();
		}
			
		//change focus?
		if ($(this).attr('changeFocus')!=null)
			$($(this).attr('changeFocus')).focus();
	});

	$(".print").click(function(m)
	{
		print();
	});

	
	//clear button
	$("#clear").click(function(m)
	{
		//a clear actually creates a new drawing so the old one is preserved
		send(0,"paper_Create", { "moveClients":1 } );
	});

	var save="";
	$("#savePNG").click(function(m)
	{
		save="png";
		send(0,"paper_Export");
		$("#fileBar").addClass("loading");
	});

	$("#saveSVG").click(function(m)
	{
		save="svg";
		send(0,"paper_Export");
		$("#fileBar").addClass("loading");
	});

	var shareType="";
	$(".share").click(function(m)
	{
		shareType=$(this).attr("shareType");
		send(0,"paper_Export");
		$(this).addClass("loading");
	});

	$("#authShow").click(function(m)
	{
		$('#authMsg').html("");
		$('#authForm').dialog({ 
			buttons:
			{
				'Annuleren': function()
				{ 
					$('#authForm').dialog("destroy"); 
				},
				'Inloggen als gast': function()
				{
					$('#authKey').val("");
//					send(paperModId,"paper_Authenticate",{
//						key:""
//					});
				}
			},
			'close': function ()
			{ 
			},
			modal: true
		});
		$('#authKey').focus();
	});
	
	$("#authKey").keyup(function(m)
	{
		if ($('#authKey').val()!="")
		{
//			send(paperModId,"paper_Authenticate",{
//				key:$('#authKey').val()
//			});
		}					
		$('#authMsg').html("");
	});

	//authenticated with wrong key
	synapse_register("paper_AuthWrongKey",function(msg_src, msg_dst, msg_event, msg)
	{
		if ($('#authKey').val()=="")
		{
			$('#authMsg').html("Inloggen als gast is niet mogelijk.");
			$('#authKey').focus();
		}
		else
			$('#authMsg').html("Wachtwoord ongeldig (blijft u typen)");
	});


	
	//we also receive this if somebody else requested the save!
	synapse_register("paper_Exported",function(msg_src, msg_dst, msg_event, msg)
	{
	
		
		if (shareType!="")
		{
			$(".loading").removeClass("loading");

			//addthis base url:
			var addThis="http://api.addthis.com/oexchange/0.8/";
			
			if (shareType=="addthis")
				addThis+="offer?";
			else
				addThis+="forward/"+shareType+"/offer?";
			
			addThis+="url="+escape(document.location)+"&";
			addThis+="title="+escape("Internet papier #"+getUrlId())+"&";
			addThis+="username=psy0rz&";
			window.open(addThis);
			shareType="";
		}
		
		if (save!="")
		{
			$(".loading").removeClass("loading");

			if (save=="png")
				window.open(msg["pngPath"]);
			if (save=="svg")
				window.open(msg["svgPath"]);
			
			save="";
		}

	});
			

	//wait until svgloading is complete
    window.addEventListener('SVGLoad', function() {
	    
		//create a svg node and assign it to global variable "drawing".
		drawing = document.createElementNS(svgns, "svg");

		//set some basic attributes
		drawing.setAttribute("id","1000r");
		//HACK: this viewbox hack is only needed for a SVGWEB flash renderer bug. The server sends us this as well.
		drawing.setAttribute("viewBox", "0 0 "+(10000*aspectRatio)+" 10000");

		//wait until rootobject is created
		drawing.addEventListener('SVGLoad', function(evt) 
		{
			//when its in flashmode, the references change somehow, so re-get it:				
			drawing=document.getElementsByTagNameNS(svgns, 'svg')[0];

			
			//set mouse move event
			//NOTE: pageX is not set when using chromium in flash mode? when we use jquery mousemove its ok ,but then we cant see 
			//which object is hovered over.
			drawing.addEventListener('mousemove', function(m) 
			{
				mouseTarget=m.target.id;
				//work around SVG bug http://code.google.com/p/svgweb/issues/detail?id=244
				if(drawing.fake)
				{
					//flash workaround
					mousePoint.x=m.clientX;
					mousePoint.y=m.clientY;
				} 
				else 
				{
					//normal
					mousePoint.x=m.clientX-$("#drawing").offset().left;
					mousePoint.y=m.clientY-$("#drawing").offset().top ;
				}
											
				mouseMove(false);
			});

			//set mouse click event
			drawing.addEventListener('mousedown', function(m)
			{
				if (m.preventDefault)
					m.preventDefault();
				else
					m.returnValue=false;


				mouseStart(m);

				return false;
			});

			//set mouse up event
			drawing.addEventListener('mouseup', mouseEnd);
					
			//set mouse leave event
			//(doesnt work with addeventlistener!)
			$("#drawing").mouseleave(mouseEnd);

			
			
			/// start mouse update engine
			window.setInterval(function() { mouseMove(false) }, mouseInterval);

			//finally!... 
			//join the specified paper!
			send(0,"paper_Login", {
				"objectId":getUrlId(),
				"key": ""
			});
					

		});
		
		//add drawing rootobject to html container
		var drawingHtml = document.getElementById("drawing");
		console.log("tis", drawing, drawingHtml);
		svgweb.appendChild(drawing, drawingHtml);
     }, false);


	//update client rights and other info..
	synapse_register("object_Client",function(msg_src, msg_dst, msg_event, msg)
	{	
		//its our own info, update the widgets to match the state of autorisation!
		if (msg_dst==msg["clientId"])
		{
			//store rights in global array for easy access
			updateAuthorisation(msg["rights"]);
	//		if (authorized["cursor"])
	//		{
	//			//tell people who we are and set random mouse position
	//			sendDraw({
	//				'cursor':{
	//					'clientName':$("#chatClientName").val(),
	//					'x':Math.round(9000*Math.random())+1000,
	//					'y':Math.round(9000*Math.random())+1000
	//				}
	//			});
	//		}
	//
	//		});
			
		}
		
		var name=filterInput(msg.name,15);

		//update cursor text, if it exists
		var e=document.getElementById('cursorName'+msg.clientId);
		if (e)
			e.childNodes[0].data=filterInput(name,15);
		
		//update chat client list
		var e=document.getElementById('chatClient'+msg.clientId);
		if (e==null)
		{
			$("#chatClients").append("<div id='chatClient"+msg.clientId+"' class='chatClient'>"+name+"</div>");
			chatPrintOnline();
		}
		else
			e.childNodes[0].data=name;
		
	});
	
	//we've joined a object
	synapse_register("object_Joined",function(msg_src, msg_dst, msg_event, msg)
	{
		//doesnt the url id match?
	//FIXME: push history
	//	if (getUrlId()!=msg["objectId"])
	//		document.location=msg["htmlPath"]+document.location.hash;
		
		//we've joined a object, remeber the ids
		currentObjectId=msg["objectId"];
		paperModId=msg_src;
		$("#objectId").html(msg["objectId"]);
	
		//since we've joined, ask for a complete reload of the drawing
		sendDraw({
			'cmd':'reload'
		});
	
		
		document.location.hash="";
	});

});

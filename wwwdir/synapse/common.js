// Common helper functions, used in synapse and other perhaps projects
// Not required by synapse-core.
// Requires jquery.cooquery.

//work around missing/different console objects
//TODO:replace this with custom logging later perhaps.
if (typeof console == 'undefined')
{
	function Cconsole(){
	};

	console=new Cconsole();
}

if (typeof console.info == 'undefined')
	console.info=function() { };

//IE8 (and perhaps other?) has no debug, but DOES have an info, so we do it this way:
if (typeof console.debug == 'undefined')
	console.debug=console.info;

if (typeof console.log == 'undefined')
	console.log=console.info;

if (typeof console.error == 'undefined')
	console.error=console.info;

//returns a random string of length chars.
function randomStr(length) {
	var chars = "abcdefghijklmnopqrstuvwxyz0123456789";
	var s = '';
	for (var i=0; i<length; i++) {
		var offset = Math.floor(Math.random() * chars.length);
		s += chars.substring(offset,offset+1);
	}
	return(s);
}

//get a random unguessable client identifier that stays the same accross javascript instances and browser sessions
//(this one IS stored in a cookie, contrary to the authCookie which is different among javascript instances)
function getClientId()
{
	var clientId=$.readCookie('clientId', { path:"/" } );
	
	//non yet, generate a new one
	if (clientId==null)
	{
		clientId=randomStr(8);
		$.setCookie('clientId', clientId, { duration:365, path:"/" });	
	}

	return (clientId);
}

//escape stuff to make it jquery compatible. 
function escapeId(myid) { 
	if (typeof myid != 'undefined')
		return '#' + myid.replace(/([,@\/.])/g,'\\$1');
	else
		return '#undefined';
}

//escape stuff to make it jquery compatible. 
function escapeClass(myclass) { 
	if (typeof myclass != 'undefined')
		return '.' + myclass.replace(/([,@\/.])/g,'\\$1');
	else
		return '.undefined';
}




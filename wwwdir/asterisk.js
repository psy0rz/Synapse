
//TODO: make configurable
var defaultCountryCode="+31";

var loginDeviceId="";
var phoneBook={};




//get jquery object to template dom
function getTemplate(class_name, context)
{
    return($(".template."+class_name, context));

}

//clones a template and returns it as a jquery object
//removes .template class from clone.
function cloneTemplate(class_name, context)
{
  clone=getTemplate(class_name,context).clone(true,true);
  clone.removeClass("template");
  return(clone);
}

/// SYNAPSE EVENT HANDLERS
synapse_register("error",function(errortxt)
{
//          $('#status-msg').html("<span class='error'>"+errortxt+"</span>");
});

synapse_register("module_Error",function(msg_src, msg_dst, msg_event, msg)
{
    $('#server-msg').html(msg["description"]);
});

synapse_register("asterisk_State",function(msg_src, msg_dst, msg_event, msg)
{
    if (msg.state)
      $('#server-msg').html("Asterisk server '"+msg["id"]+"' is not connected.<br>"+msg["ami_error"]+"<br>"+msg["net_error"]);
    else
      $('#server-msg').html("");



});

synapse_register("module_SessionStart",function(msg_src, msg_dst, msg_event, msg)
{
    $('#status-msg').html("Requesting login...");
    //logged out
    $('.show-when-logged-out').show();
    $('.show-when-logged-in').hide();
    send(0,"asterisk_authReq", {
        authCookie: $.readCookie('asterisk_authCookie'),
        deviceId:   $.readCookie('asterisk_deviceId'),
        serverId:   $.readCookie('asterisk_serverId')
    });
    // send(0,"asterisk_refresh", {
    //     authCookie: $.readCookie('asterisk_authCookie'),
    //     deviceId:   $.readCookie('asterisk_deviceId'),
    //     serverId:   $.readCookie('asterisk_serverId')
    // });

});

synapse_register("asterisk_authCall",function(msg_src, msg_dst, msg_event, msg)
{
    $('#status-msg').html("Please login by calling: <b>"+msg["number"]+"</b>");
    //logged out
    $('.show-when-logged-out').show();
    $('.show-when-logged-in').hide();
});

synapse_register("asterisk_authOk",function(msg_src, msg_dst, msg_event, msg)
{
    $('#status-msg').html("Downloading data...");
    //logged in
    $('.show-when-logged-out').hide();
    $('.show-when-logged-in').show();
    loginDeviceId=msg["deviceId"];

    //store the device and authcookie in a browser cookie, so the user doesnt have to relogin everytime.
    $.setCookie('asterisk_authCookie',msg["authCookie"], {duration:365, path:"/"});
    $.setCookie('asterisk_deviceId',msg["deviceId"], {duration:365, path:"/"});
    $.setCookie('asterisk_serverId',msg["serverId"], {duration:365, path:"/"});

    //request a refresh, to learn about all objects
    send(0,"asterisk_refresh", {
    });
});

synapse_register("asterisk_reset",function(msg_src, msg_dst, msg_event, msg)
{
    //reset all known state info
    //(new info probably follows because we send a asterisk_refresh request)
    $('.device:not(.template)').remove();
    $('.channel:not(.template)').remove();
    $('.speed-dial-entry:not(.template)').remove();

});

synapse_register("asterisk_resetComplete",function(msg_src, msg_dst, msg_event, msg)
{
    $('#status-msg').html("");
});

function normalizeNumber(number)
{
    var normalizedNumber=number.replace(/[^0-9+]/, "");
    normalizedNumber=normalizedNumber.replace(/^0/, defaultCountryCode);
    return(normalizedNumber);
}

function findPhoneBookEntry(number)
{
    if (number.length<=4)
        return(undefined);

    var normalizedNumber=normalizeNumber(number);

    for (var groupId in phoneBook)
    {
        for (var phoneBookI in phoneBook[groupId])
        {
            if (normalizeNumber(phoneBook[groupId][phoneBookI]["number"])==normalizedNumber)
            {
                return(phoneBook[groupId][phoneBookI]);
            }
        }
    }
    return(undefined);
}

function prettyCallerId( callerId, callerIdName)
{
    var txt="";

    //try to find in phonebook first
    var phoneBookEntry=findPhoneBookEntry(callerId);
    if (phoneBookEntry)
    {
        txt=phoneBookEntry["first_name"]+" "+phoneBookEntry["last_name"]+" ("+phoneBookEntry["company"]+")";
    }
    else
    {
        if (callerIdName!="")
            txt=txt+callerIdName;

        if (callerId!="")
            txt=txt+" &lt;"+callerId+"&gt;";
        else
            txt=" (unknown)";
    }

    return txt;
}

//add a dom html object to a list but use sorting.
function htmlAddSorted(html, selector, context, sort_string)
{
    var items=$(selector, context);

    html.attr("sort-string",sort_string);

    //find out sort order
    var added=false;
    items.each(function(index, item)
    {
        if ($(item).attr("sort-string") > sort_string)
        {
            html.insertBefore(item);
            added=true
            return(false);
        }
    });

    if (!added)
        context.append(html);
}


//received information about a new or existing device
synapse_register("asterisk_updateDevice",function(msg_src, msg_dst, msg_event, msg)
{
    var device;
    var html;
    var template;

    device=$(escapeId(msg["id"]));

    //device doesnt exist yet?
    if (device.length==0)
    {
        //is it the device of the logged in user?
        if (msg["id"]==loginDeviceId)
        {
            html=cloneTemplate("device-own");
            html.attr("id", msg["id"]);
            $('#device-user').append(html);
        }
        else
        {
            html=cloneTemplate("device-other");
            html.attr("id", msg["id"]);
            htmlAddSorted(html, ".sorted-button", $("#device-list"), msg.callerIdName);

        }
        device=$(escapeId(msg["id"]));
    }

    //store all the info as data in the device dom object:
    device.data("device", msg);

    //update the device caller id:
    $(".device-info", device).html(prettyCallerId(msg["callerId"], msg["callerIdName"]));

    if (msg["online"])
        $(device).removeClass('ui-state-disabled');
    else
        $(device).addClass('ui-state-disabled');
});


synapse_register("asterisk_delDevice",function(msg_src, msg_dst, msg_event, msg)
{
    $(escapeId(msg["id"])).slideUp(100,function() {
        $(this).remove();
    });
});



var blink_ring_status=false;
var blink_hold_status=false;
var blink_count=0;
function blinker()
{
    blink_count++;
    setTimeout(blinker, 125);

    //ring blinker (fast)
    blink_ring_status=!blink_ring_status;
    if (blink_ring_status)
        $('.device .channel-icon[state="ring_in"]').addClass("ring-blinker");
    else
        $('.ring-blinker').removeClass("ring-blinker");

    //hold blinker (slow)
    if (blink_count%4 == 0)
    {
        blink_hold_status=!blink_hold_status;
        if (blink_hold_status)
            $('.channel-icon[state="hold"]').addClass("hold-blinker");
        else
            $('.hold-blinker').removeClass("hold-blinker");
    }
}
blinker();


//create/updates channels for .device objects (devices of both owner and other users)
function update_device_channel(msg, callerInfo)
{
    var moved=false;

    //does the channel exist already?
    if ($(".device "+escapeClass(msg["id"])).length!=0)
    {
        //doesnt it exist under the specified device?
        if ($(escapeId(msg["deviceId"]) + " " + escapeClass(msg["id"])).length==0)
        {
             //then it probably has been moved/renamed, so delete the channel and recreate it below.
            moved=true; //since it takes a while for the effect to actually remove it:
            $(".device "+escapeClass(msg["id"])).slideUp(100,function() {
                $(this).remove();
            });
        }
    }

    var device=$(escapeId(msg["deviceId"]));

    //channel doesnt exist (anymore)?
    if (moved || $(escapeClass(msg["id"]), device).length==0)
    {
        var template=getTemplate("channel", device);
        var html=cloneTemplate("channel", device);
        html.addClass(msg["id"]);
        html.insertBefore(template);
        // $(escapeId(msg["deviceId"])+' .channel-list').append(newChannelHtml);
    }

    //store all the info as data in the channel dom object:
    $(escapeClass(msg["id"]), device).data("channel", msg);
    $(escapeClass(msg["id"])+" .channel-info", device).html(callerInfo);
}



//channels that are parked by us should show up in the parking area
function update_parked_channel(msg, callerInfo)
{
    ///////////////////////////// special parked stuff
    //the channel is parked by us?
    if (msg["ownerDeviceId"]==loginDeviceId)
    {

        //parked channel doesnt exist yet?
        if ($(escapeClass(msg["id"])+"_parked").length==0)
        {
            //add a new parked channel object
            newChannelHtml=cloneTemplate("channel-parked");
            newChannelHtml.addClass(msg["id"]+"_parked");
            $("#channel-parked-list").append(newChannelHtml);


            $(escapeClass(msg["id"])+'_parked .channel-icon').attr('state', "hold");
            $(escapeClass(msg["id"])+'_parked').slideDown(100);
        }

        //update called id of the parked channel
        $(escapeClass(msg["id"])+"_parked .channel-info").html(prettyCallerId(msg["callerId"], msg["callerIdName"]));
        //store all data
        $(escapeClass(msg["id"])+"_parked").data("channel", msg);


    }
    //its not parked (by us), so remove it from our list if its stil there
    else
    {
        $(escapeClass(msg["id"])+"_parked").slideUp(100,function() {
            $(this).remove();
        });
    }

}


var notification_popup=null;


function update_popup_channel(msg, callerInfo)
{
    if (document.hasFocus())
    {
        return;
    };

    //is it our channel?
    if (msg["deviceId"]==loginDeviceId)
    {
        //its ringing?
        if (msg["state"]=="ring_in")
        {
            //no popup anymore?
            if (notification_popup==null || notification_popup.closed)
            {
                 notification_popup=window.open("asterisk_popup.html", "asterisk_popup", "height=100,width=100");
            }
            else
            {
                notification_popup.focus();
            }

            $(notification_popup).load(function()
            {

                $("#status-msg", notification_popup.document).html(callerInfo);
            });
        }
    }
}

synapse_register("asterisk_updateChannel",function(msg_src, msg_dst, msg_event, msg)
{
    //determine caller info
    var callerInfo="";

    //is channel parked by other side?
    if (msg["ownerDeviceId"])
    {
        callerInfo+=prettyCallerId(msg["callerId"], msg["callerIdName"]);
        callerInfo+=" [parked by "+$(escapeId(msg["ownerDeviceId"])).data("device")['callerIdName']+"]";
    }
    //not parked (anymore)
    else
    {
        //we know who's connected:
        if (msg["linkCallerId"] != '')
        {
            callerInfo+=prettyCallerId(msg["linkCallerId"], msg["linkCallerIdName"]);
        }
        //we dont know whos connected yet
        else
        {
            //show who's being dialed, or show caller id if its an incoming call:
            // if (msg["state"]=="out")
            //     callerInfo+="Calling "+msg["firstExtension"];
            // else
                callerInfo+=prettyCallerId(msg["callerId"], msg["callerIdName"]);
        }
    }


    update_device_channel(msg, callerInfo);
    update_parked_channel(msg, callerInfo);
    update_popup_channel(msg, callerInfo);

    //update all channel-icons
    $(escapeClass(msg["id"])+" .channel-icon").attr('state', msg["state"]);



});


synapse_register("asterisk_delChannel",function(msg_src, msg_dst, msg_event, msg)
{
    //remove channel from device
    $(".device "+escapeClass(msg["id"])).slideUp(100,function() {
        $(this).remove();
//          //if device is now free, remove the highlight
        // if ($(escapeId(msg["deviceId"])+" .channel").length==0)
        // {
        //  $(escapeId(msg["deviceId"])).removeClass("ui-state-error");
        // }

    });

    //in case its parked, remove the parked fake channel as well
    $(escapeClass(msg["id"])+"_parked").slideUp(100,function() {
        $(this).remove();
    });

    //hide it from the call list with a nice effect
    $("#active-call-list "+escapeClass(msg["id"])).slideUp(100,function() {
        //add class and move it to the history
        $(this).addClass('deleted');
        //yes, this will actually MOVE the object, not copy it:
        $("#history-call-list .list").prepend($(this));
        //show it again with a nice effect
        $("#history-call-list " + escapeClass(msg["id"])).slideDown(100);
    });

});


synapse_register("module_SessionEnded",function(msg_src, msg_dst, msg_event, msg)
{
});

//filter spaces and (0) stuff from numbers before dialing
function filterNumber(number)
{
        number=number.replace(/ /g, "");
        number=number.replace(/\(0\)/g,"");
        return(number);
}

/// JAVA SCRIPT EVENT HANDLERS
$(document).ready(function(){

    window.name="asterisk";

    //logged out
    $('.show-when-logged-out').show();
    $('.show-when-logged-in').hide();

    // $("#device-list").sortable();
    $("#device-list").disableSelection();
    $("#logout").click(function()
    {
        send(0,"asterisk_authReq", {});
        $.delCookie('asterisk_authCookie');
        $.delCookie('asterisk_deviceId');
        $.delCookie('asterisk_serverId');
    });

    $("#login-user").click(function()
    {
        synapse_login();
    });

    $('.dial-button').on('click', function()
    {
        asterisk_Call($('.dial-number', $(this).parent() ).val());

    });

    //gets an active channel from our device thats not on hold and not ringin in or out
    //returns all channel data
    function getActiveChannel()
    {

        var channels=$(escapeId(loginDeviceId)+" .channel:not(.template)");
        var activeChannel={};

        channels.each(function(index, channel)
        {
            var data=$(channel).data("channel");
            if (!$(channel).hasClass("parked_channel") && data["state"]!="hold")
            {
                activeChannel=data;
                return(false);
            }
        });
        return(activeChannel);
    }

    //place an new call:
    //-tries reuse channels active channels and parking the active call
    //-filters number using filterNumber()
    function asterisk_Call(number)
    {
        //hang up all unaswered originating calls
        var channels=$(escapeId(loginDeviceId)+" .channel:not(.template)");

        var reuseChannelId=getActiveChannel()["id"];

        send(0, "asterisk_Call",
        {
            "exten": filterNumber(number),
            "reuseChannelId": reuseChannelId,
            "parkLinked": true
        });

    }

    $("body").on("click", ".speed-dial-entry", function()
    {
        asterisk_Call($(this).attr("number"));
    });


    ///////////////////// device button
    $("#device-list").on("click", ".device", function()
    {
        asterisk_Call($(this).data("device")["exten"]);
    });


    ////////////////// channel button stuff
    $("body").on("click", ".channel-menu-item", function()
    {

        var channel=$(this).parent().data("channel");
        var activeChannel=getActiveChannel();

        if ($(this).hasClass("channel-menu-item-hang-up"))
        {
            send(0, "asterisk_Hangup", {
                "channel": channel["id"]
            });
        }
        else if ($(this).hasClass("channel-menu-item-transfer"))
        {
            send(0, "asterisk_Bridge",
            {
                "channel1": activeChannel["linkId"],
                "channel2": channel["id"],
                "parkLinked1": false, //NOTE: it would be nice if the channel would stay open in some cases?
            });
        }
        else if ($(this).hasClass("channel-menu-item-hold"))
        {
            send(0, "asterisk_Park",
            {
                "channel1": channel["linkId"],
            });
        }
        else if ($(this).hasClass("channel-menu-item-resume"))
        {
            send(0, "asterisk_Bridge",
            {
                "channel1": activeChannel["id"],
                "channel2": channel["id"],
                "parkLinked1": true,
            });
        }

        return(false);
    });

    //remove menus when user clicks anywere else
    $(document).on("click", function()
    {
        $(".menu").remove();
    });
});



//(debugging is disabled on server side)
synapse_register("asterisk_debugChannel",function(msg_src, msg_dst, msg_event, msg)
{

    if ($(escapeId("debug_"+msg["id"])).length==0)
    {
        html="<div class='channel-debug' id='debug_"+msg["id"]+"'>";
        html+="</div>";

        $('body').append(html);
    }

    var channel_debug=$(escapeId("debug_"+msg["id"]));

    channel_debug.append(
        "<br><b>"+msg["Event"]+"</b><br>"
    );
    for (var msgI in msg)
    {
        if (msgI!="id" && msgI!="serverId" && msgI!="Event")
        {
            channel_debug.append(
                msgI+":"+msg[msgI].replace("<","&lt;").replace(">","&gt;")+"<br>"
            );
        }
    }
});


synapse_register("asterisk_speedDialList",function(msg_src, msg_dst, msg_event, msg)
{
    //too GUI specific to handle here
    ;

});

synapse_register("asterisk_phoneBookList",function(msg_src, msg_dst, msg_event, msg)
{
        phoneBook[msg["groupId"]]=msg["numbers"];

});

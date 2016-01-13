
loginDeviceId="";

// function cloneTemplate(class_name)
// {
//
// }

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
    send(0,"asterisk_authReq", {
        authCookie: $.readCookie('asterisk_authCookie'),
        deviceId:   $.readCookie('asterisk_deviceId'),
        serverId:   $.readCookie('asterisk_serverId')
    });

});

synapse_register("asterisk_authCall",function(msg_src, msg_dst, msg_event, msg)
{
    $('#status-msg').html("Please login by calling: <b>"+msg["number"]+"</b>");
});

synapse_register("asterisk_authOk",function(msg_src, msg_dst, msg_event, msg)
{
    $('#status-msg').html("");
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
    $('.device').remove();
    $('.channel').remove();

});

function prettyCallerId( callerId, callerIdName)
{
    var txt="";
    if (callerIdName!="")
        txt=txt+callerIdName;

    if (callerId!="")
        txt=txt+" &lt;"+callerId+"&gt;";
    else
        txt=" (unknown)";

    return txt;
}




synapse_register("asterisk_updateDevice",function(msg_src, msg_dst, msg_event, msg)
{

    //device doesnt exist yet?
    if ($(escapeId(msg["id"])).length==0)
    {
        //add dom objects
        html="<div class='ui-state-default ui-widget-content ui-corner-all device' id='"+msg["id"]+"'>";
        html+="<div class='device-info'></div>";
        html+="<div class='channel-list'></div>";
        html+="</div>";

        //is it the device of the logged in user?
        if (msg["id"]==loginDeviceId)
            $('#status-user').append(html);
        else
            $('#device-list').append(html);


    }


    //store all the info as data in the device dom object:
    $(escapeId(msg["id"])).data("device", msg);

    //update the device caller id:
//          $(escapeId(msg["id"])+" .device-info").html(msg["callerId"]+"<br> tennant="+msg["groupId"]+" ("+msg["id"]+")");
    $(escapeId(msg["id"])+" .device-info").html(prettyCallerId(msg["callerId"], msg["callerIdName"]));

    if (msg["online"])
        $(escapeId(msg["id"])).removeClass('ui-state-disabled');
    else
        $(escapeId(msg["id"])).addClass('ui-state-disabled');




});


synapse_register("asterisk_delDevice",function(msg_src, msg_dst, msg_event, msg)
{
    $(escapeId(msg["id"])).slideUp(100,function() {
        $(this).remove();
    });
});




synapse_register("asterisk_debugChannel",function(msg_src, msg_dst, msg_event, msg)
{
    //TODO: make it possible to disable debugging, its slow
    //add debug dom object?
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
        $('.device .channel-icon[state="ringing"]').addClass("ring-blinker");
    else
        $('.ring-blinker').removeClass("ring-blinker");

    //hold blinker (slow)
    if (blink_count%4 == 0)
    {
        blink_hold_status=!blink_hold_status;
        if (blink_hold_status)
            $('.device .channel-icon[state="hold"]').addClass("hold-blinker");
        else
            $('.hold-blinker').removeClass("hold-blinker");
    }
}
blinker();


synapse_register("asterisk_updateChannel",function(msg_src, msg_dst, msg_event, msg)
{
    var moved=false;

    /////////// DeviceList-only stuff:
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

    //channel doesnt exist (anymore)?
    if (moved || $(".device "+escapeClass(msg["id"])).length==0)
    {
        //add a brand new channel object
        var newChannelHtml="";
        newChannelHtml+="<div class='channel "+msg["id"]+"'>";
        newChannelHtml+="<span class='channel-icon'></span>";
        newChannelHtml+="<span class='channel-info'></span>";

        //is it our device?
        if (msg["deviceId"]==loginDeviceId)
        {
            newChannelHtml+="<span class='channel-menu-item channel-menu-itemHangup'>End call</span>";
            newChannelHtml+="<span class='channel-menu-item channel-menu-item-hold'>Hold</span>";
        }


        newChannelHtml+="</div>";
        $(escapeId(msg["deviceId"])+' .channel-list').append(newChannelHtml);
    }

    //store all the info as data in the channel dom object:
    $(".device "+escapeClass(msg["id"])).data("channel", msg);

    //set caller info
    var callerInfo="";

    //channel is parked?
    if (msg["ownerDeviceId"])
    {
        callerInfo="[parked by "+$(escapeId(msg["ownerDeviceId"])).data("device")['callerIdName']+"]";
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
            if (msg["state"]=="out")
                callerInfo+="Calling "+msg["firstExtension"];
            else
                callerInfo+=prettyCallerId(msg["callerId"], msg["callerIdName"]);
        }
    }

    $(".device "+escapeClass(msg["id"])+" .channel-info").html(callerInfo);


    ////////////////////// active-call-list-only stuff:
    //add new channel to active call list?
    if ($("#active-call-list "+escapeClass(msg["id"])).length==0)
    {
        var newChannelHtml="";
        newChannelHtml+="<div class='channel "+msg["id"]+"'>";
        newChannelHtml+="<span class='channel-icon'></span>";
        newChannelHtml+="<span class='channel-info'></span>";
        newChannelHtml+="</div>";
        $("#active-call-list .list").prepend(newChannelHtml);
        show=1;

        //make the device highlight when hoovering the active-call-list channel
        $("#active-call-list "+escapeClass(msg["id"])).hover(
            function () {
                $(this).addClass("ui-state-highlight");
                $(escapeId(msg["deviceId"])).addClass("ui-state-highlight");

                //if its in the deviceList, scroll to it:
                var device=$("#device-list "+escapeId(msg["deviceId"]));
                if (device.length!=0)
                {
                    $('#device-list').scrollTop($('#device-list').scrollTop()+device.position().top);
                }
            },
            function () {
                $(this).removeClass("ui-state-highlight");
                $(escapeId(msg["deviceId"])).removeClass("ui-state-highlight");
            }
        );

        //when clicking, show the corresponding debug object
        $("#active-call-list "+escapeClass(msg["id"])).click(
            function () {
                $(escapeId("debug_"+msg["id"])).dialog({
                    title: "Debug channel "+msg["id"],
                    position: "top"

                });
            }
        );
    }

    //update active call list
    $("#active-call-list "+escapeClass(msg["id"])+" .channel-info").html(
        prettyCallerId(msg["deviceCallerId"], msg["deviceCallerIdName"]) + " ... "+callerInfo
    );


    ///////////////////// Stuff for all channels
    $(escapeClass(msg["id"])+" .channel-icon").attr('state', msg["state"]);
    $(escapeClass(msg["id"])+":hidden").slideDown(100); //show the channel (default is hidden)


    ///////////////////////////// special parked stuff
    //the channel is parked by us?
    if (msg["ownerDeviceId"]==loginDeviceId)
    {

        //we create a fake parked channel in our device to show the parked call.

        //parked channel doesnt exist yet?
        if ($(escapeClass(msg["id"])+"_parked").length==0)
        {
            //add a new parked channel object
            var newChannelHtml="";
            newChannelHtml+="<div class='channel parked_channel "+msg["id"]+"_parked'>";
            newChannelHtml+="<span class='channel-icon'></span>";
            newChannelHtml+="<span class='channel-info'></span>";
            newChannelHtml+="<span class='channel-menu-item channel-menu-itemHangup'>End call</span>";
            newChannelHtml+="<span class='channel-menu-item channel-menu-item-resume'>Resume</span>";
            newChannelHtml+="<span class='channel-menu-item channel-menu-item-transfer'>Transfer</span>";
            newChannelHtml+="</div>";
            $(escapeId(loginDeviceId)+' .channel-list').append(newChannelHtml);

            $(escapeClass(msg["id"])+'_parked .channel-icon').attr('state', "hold");
            $(escapeClass(msg["id"])+'_parked').slideDown(100);
        }

        //update called id of the parked channel
        $(escapeClass(msg["id"])+"_parked .channel-info").html(prettyCallerId(msg["callerId"], msg["callerIdName"]));
        //store all data
        $(escapeClass(msg["id"])+"_parked").data("channel", msg);


    }
    //its not parked (by us), so remove it from our list
    else
    {
        $(escapeClass(msg["id"])+"_parked").slideUp(100,function() {
            $(this).remove();
        });
    }




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

/// JAVA SCRIPT EVENT HANDLERS
$(document).ready(function(){

    $("#device-list").sortable();
    $("#device-list").disableSelection();
    $("#login").click(function()
    {
        send(0,"asterisk_authReq", {});
        //send(0,"asterisk_reset", {});
    });

    $("#login-user").click(function()
    {
        synapse_login();
    });

    $('#test1').on('click', function()
    {
        // send(0, "asterisk_Test", {
        //  "Action"    : "Originate",
        //  "Channel"   : "SIP/100",
        //  "Context"   : "from-synapse",
        //  "Priority"  : 1,
        //  "Exten"     : 901,
        // });

    });


    //gets an active channel from our device thats not on hold
    //returns all channel data
    function getActiveChannel()
    {

        var channels=$(escapeId(loginDeviceId)+" .channel");
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

    $("#device-list").on("click", ".device", function()
    {
        console.log("Clicked device", $(this).data("device"));

        var reuseChannelId=getActiveChannel()["id"];

        send(0, "asterisk_Call",
        {
            "exten": $(this).data("device")["exten"],
            "reuseChannelId": reuseChannelId
        });
    });


    ////////////////// channel button stuff
    $("body").on("click", ".channel-menu-item", function()
    {

        var channel=$(this).parent().data("channel");
        var activeChannel=getActiveChannel();

        if ($(this).hasClass("channel-menu-itemHangup"))
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

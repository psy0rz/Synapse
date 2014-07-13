
synapse_register("error",function(errortxt)
{
    $('#error').html(errortxt);
}); 

    
var timeOffset=0;

function serverTime()
{
    return ( ((new Date()).getTime()/1000) + timeOffset);
}

synapse_register("module_SessionStart",function(msg_src, msg_dst, msg_event, msg)
{
    timeOffset=Math.round( msg["time"]-((new Date()).getTime()/1000));
    console.debug("Server time offset: ", timeOffset);

    //style buttons and give them icons
    $("button").each(function()
    {
        $(this).button({
            icons: { 
                primary: $(this).attr("icon")
            }
        });
    });



    send(1, "core_Login", { 
        username: "admin",
        password: "as"
    });

});

synapse_register("module_Login",function(msg_src, msg_dst, msg_event, msg)
{
    send(0, "arduino_Refresh", {
    });

});

synapse_register("arduino_Received",function(msg_src, msg_dst, msg_event, msg)
{
    //every node/event combination gets a widget, cloned from the prototype
    var widget_id=msg["node"]+"_"+msg["event"].replace(".","_");
    var widget_element=$("#"+widget_id);

    //widget_element doesnt exist yet?
    if (widget_element.length==0)
    {
        //do we have a proto type for this event?
        var prototype_element=$('.prototype[event="'+msg["event"]+'"]' );
        if (prototype_element.length!=0)
        {
            //clone the prototype
            widget_element=prototype_element.clone(true,true);
            widget_element.insertAfter(prototype_element);
            widget_element.removeClass("prototype");
            widget_element.attr("id", widget_id);
            widget_element.attr("node", msg["node"]);
        }

        //call the create-handler of the widget
        widget_element.trigger("widget_created", msg);
    }

    //call the update-handler of the widget
    widget_element.trigger("widget_update", msg);


});


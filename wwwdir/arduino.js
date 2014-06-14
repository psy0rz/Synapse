
synapse_register("error",function(errortxt)
{
    $('#error').html(errortxt);
}); 

        
synapse_register("module_SessionStart",function(msg_src, msg_dst, msg_event, msg)
{
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
    console.log(msg);

    //every node/event combination gets a widget, cloned from the prototype
    var widget_class=msg["node"]+"_"+msg["event"].replace(".","_");
    var widget_element=$("."+widget_class);

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
            widget_element.addClass(widget_class);
        }

        //call the create-handler of the widget
        widget_element.trigger("widget_created", msg);
    }

    //call the update-handler of the widget
    var widget_element=$("."+widget_class);
    widget_element.trigger("widget_update", msg);


});


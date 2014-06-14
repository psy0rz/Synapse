
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


    $(".pl_mode").buttonset();

    //time slider
    $(".time").slider({ 
        min: 0,
        slide: function (event, ui)
        {
            send(0,"play_SetTime", { 'time': ui.value, 'ids': player_ids } );
        }
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
});


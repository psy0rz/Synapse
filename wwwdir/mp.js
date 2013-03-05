
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
            send(0,"play_SetTime", { 'time': ui.value } );
        }
    });

    send(1, "core_Login", { 
        username: "admin",
        password: "as"
    });

});

synapse_register("module_Login",function(msg_src, msg_dst, msg_event, msg)
{
    //get status from player
    send(0, "play_GetStatus", { 
    });

    //get status from playlist
    send(0, "pl_GetStatus", { 
    });

    //translate clicks on elements that have the send-class to a synapse event:
    $(".send").click(function() 
    {
        send(0, $(this).attr("event"));
    });


    $("#pl_mode_filename").click(function()
    {
        send(0, "pl_SetMode", {
            "sortField":"filename",
            "randomLength":0
        });
    });

    $("#pl_mode_date").click(function()
    {
        send(0, "pl_SetMode", {
            "sortField":"date",
            "randomLength":0
        });
    });

    $("#pl_mode_random").click(function()
    {
        send(0, "pl_SetMode", {
            "sortField":"filename",
            "randomLength":10000
        });
    });


    $('.scroll-send').bind('mousewheel DOMMouseScroll', function(e){
//            e.stopImmediatePropagation();
//        e.stopPropagation();
      e.preventDefault();

        var delta = parseInt(e.originalEvent.wheelDelta || -e.originalEvent.detail);
        console.log(delta);
        if(delta >0) 
        {
            //scrolled up
            send(0, $(this).attr("up-event"))
        }
        else
        {            
            //scrolled down
            send(0, $(this).attr("down-event"))
        }

        return(false);
    });

    var lastFileFilter="";
    $('.pl_SetFileFilter').on('keyup',function()
    {
        if (lastFileFilter!=$(this).val())
        {
            lastFileFilter=$(this).val();
            send(0, "pl_SetFileFilter", {
                "fileFilter": lastFileFilter
            });
        }

        return (false);
    }); 

/*    $(':input').on('keydown',function(event)
    {
        event.stopPropagation();
    });
*/

    $( "#deleteForm" ).dialog({
        autoOpen: false,
        modal: true,
        buttons: {
            "Delete": function() {
                send(0, "pl_DeleteFile", {});
                $(this).dialog("close");
            },
            "Cancel":function()
            {
                $(this).dialog("close");
            }
        },
        close: function(){
                $(".favorite").removeClass("ui-state-highlight");
        }
    });

    $(".pl_DeleteFile").on('click', function(event)
    {
        $("#deleteForm").dialog("open");
    });


    $( "#saveFavoriteForm" ).dialog({
        autoOpen: false,
        modal: true,
        buttons: {
            "Save favorite": function() {
                send(0, "pl_SaveFavorite", {
                    "id": $("#saveFavoriteId").val(),
                    "desc": $("#saveFavoriteDesc").val()
                });
                $(this).dialog("close");
            },
            "Cancel":function()
            {
                $(this).dialog("close");
            }
        },
        close: function(){
                $(".favorite").removeClass("ui-state-highlight");
        }
    });


    $(".favorite").on('click', function(event)
    {

        send(0, "pl_LoadFavorite", {
            "id": $(this).attr("favorite-id")
        });
    });

    $(".favorite").on('contextmenu', function(event)
    {
        $(this).addClass("ui-state-highlight");
        $("#saveFavoriteDesc").val($(this).text());
        $("#saveFavoriteId").val($(this).attr("favorite-id"));
        $("#saveFavoriteForm").dialog("open");
        return(false);
    });



    $(document).on('keydown', function(event)
    {
        if (event.altKey || event.shiftKey || event.ctrlKey || event.metaKey)
            return;

        if ($(event.target).is("button, input"))
            return;

        if (event.which == $.ui.keyCode.SPACE)
            send(0, "play_Pause", {});

        else if (event.which == $.ui.keyCode.ENTER)
            send(0, "pl_EnterPath", {});

        else if (event.which == $.ui.keyCode.BACKSPACE)
            send(0, "pl_ExitPath", {});

        else if (event.which == $.ui.keyCode.PAGE_UP)
            send(0, "pl_PreviousPath", {});

        else if (event.which == $.ui.keyCode.PAGE_DOWN)
            send(0, "pl_NextPath", {});

        else if (event.which == $.ui.keyCode.DOWN)
            send(0, "pl_Next", {});

        else if (event.which == $.ui.keyCode.UP)
            send(0, "pl_Previous", {});

        else if (event.which == $.ui.keyCode.LEFT)
            send(0, "play_MoveTime", { "time":"-10" });

        else if (event.which == $.ui.keyCode.RIGHT)
            send(0, "play_MoveTime", { "time":"10" });

        else if (event.which>=48 && event.which<=57)
            send(0, "pl_LoadFavorite", { id: event.which-47 });

        else if (event.which==81) //q
            send(0, "pl_LoadFavorite", { id: 11 });

        else if (event.which==87) //w
            send(0, "pl_LoadFavorite", { id: 12 });

        else if (event.which==69) //e
            send(0, "pl_LoadFavorite", { id: 13 });

        else if (event.which==82) //r
            send(0, "pl_LoadFavorite", { id: 14 });

        else if (event.which==84) //t
            send(0, "pl_LoadFavorite", { id: 15 });

        else if (event.which==89) //y
            send(0, "pl_LoadFavorite", { id: 16 });

        else if (event.which==$.ui.keyCode.DELETE) 
            $("#deleteForm").dialog("open");


        console.log(event);

    });

    $(".play_MoveTime10r").click(function()
    {
        send(0, "play_MoveTime", { "time":"-10" });
    });

    $(".play_MoveTime10f").click(function()
    {
        send(0, "play_MoveTime", { "time":"10" });
    });



    console.log(event);
});

synapse_register("pl_Entry",function(msg_src, msg_dst, msg_event, msg)
{
    if (msg["updateListsAsyncFlying"])
        $(".show_when_busy").css('visibility', 'visible');
    else
        $(".show_when_busy").css('visibility', 'hidden');

    $('.entry[key="rootPath"]').text(msg.rootPath);


    if (msg["randomLength"]!=0)
    {
        $("#pl_mode_random").prop('checked', true);
    }
    else if (msg["sortField"]=="date")
    {
        $("#pl_mode_date").prop('checked',true);
    }
    else if (msg["sortField"]=="filename")
    {
        $("#pl_mode_filename").prop('checked',true);
    }

    $(".pl_mode").buttonset("refresh");


    if ('currentPath' in msg)
    {

        var strippedParentPath=msg.parentPath.substr(msg.rootPath.length);
        if (strippedParentPath.length)
            $('.entry[key="parentPath"]').text(strippedParentPath);
        else
            $('.entry[key="parentPath"]').html("&nbsp;");


        $('.entry[key="currentPath"]').text(msg.currentPath.substr(msg.parentPath.length));

        if ('currentFile' in msg)
            $('.entry[key="currentFile"]').text(msg.currentFile.substr(msg.currentPath.length));


        //some dom magic going on here...for more of it and a better way to do it look at the kmur2 project
        function listClone(src, list, strip, reverse)
        {
            var color=255;
            var colorStep=(255-70)/(list.length-1);
            for (nr=0; nr<list.length; nr++)
            {
                var element=$(src).clone();
                
                if (reverse)
                    src.after(element);
                else
                    src.before(element);

                element.addClass("cloned");
                element.css('color', 'rgb('+0+','+color+','+0+')');
                color=Math.round(color-colorStep);

                var t=list[nr].substr(strip);
                if (t=="")
                    $(".entry_text", element).html("&nbsp;");
                else
                    $(".entry_text", element).text(t);
            }

            //fill the rest to the minimum length with whitespace
            for (nr=0; nr<(5-list.length); nr++)
            {
                var element=$(src).clone();
                
                if (reverse)
                    src.after(element);
                else
                    src.before(element);

                element.addClass("cloned");
                $(".entry_text", element).html("&nbsp;");
            }

        }

        //delete old clones
        $(".cloned").remove();


        listClone(
                $('.entry_clone[key="nextFiles"]'), 
                msg.nextFiles, 
                msg.currentPath.length,
                false);
            
        listClone(
                $('.entry_clone[key="prevFiles"]'), 
                msg.prevFiles, 
                msg.currentPath.length,
                true);


        listClone(
                $('.entry_clone[key="nextPaths"]'), 
                msg.nextPaths, 
                msg.parentPath.length,
                false);

        listClone(
                $('.entry_clone[key="prevPaths"]'), 
                msg.prevPaths, 
                msg.parentPath.length,
                true);

        for (key in msg.favorites)
        {
            $('.favorite[favorite-id="'+key+'"]').text(msg.favorites[key]);
        }

    }

    //filter crap from a filename so it can be googled
    function filter_crap(s)
    {
        return (
            s.replace(/\./g, " ") //convert dots to spaces
                .replace(/-/g, " ") //convert - to spaces
                .replace(/(480|720|1080|bluray|bdrip|dvdrip|xvid).*/gi, "") //remove crap
                .replace(/_/g, " ") //convert underscores
            );
    }

    if (msg["currentFile"])
    {
        //update google for file name
        var searchText=msg["currentFile"]
            .replace(/.*\//, "") //remove path
            .replace(/\.[^.]*$/, "") //remove ext

        searchText=filter_crap(searchText); 
        $(".google_file").text("Google: "+searchText);
        $(".google_file").attr("href", "https://www.google.com/search?q="+searchText);

        //update google for path name
        searchText=msg["currentFile"]
            .replace(/\/[^/]*$/, "") //remove file
            .replace(/.*\//, "") //remove path

        searchText=filter_crap(searchText); 
        $(".google_path").text("Google: "+searchText);
        $(".google_path").attr("href", "https://www.google.com/search?q="+searchText);
    }

    $('.meta[key="fileFilter"]').text(msg["fileFilter"]);

    if ($(".pl_SetFileFilter:focus").length==0)
        $(".pl_SetFileFilter").val(msg["fileFilter"]);
});

synapse_register("play_Time",function(msg_src, msg_dst, msg_event, msg)
{
    function timestr(seconds)
    {
        var str=Math.round(seconds/60)+":";

        if (seconds%60<10)
            str=str+"0";
        str=str+(seconds%60);

        return(str);
    }

    $(".time").slider("value", msg['time']);
    $(".time").slider("option", "max", msg['length']);

    $('.meta[key="time_pos"]').text(timestr(msg.time));

    if (msg.length==0)
    {
        $('.meta[key="time_length"]').text("live");
        $('.meta[key="time_left"]').text("live");
    }
    else
    {
        $('.meta[key="time_length"]').text(timestr(msg.length));
        $('.meta[key="time_left"]').text(timestr(msg.length-msg.time));
    }

});

synapse_register("play_State",function(msg_src, msg_dst, msg_event, msg)
{
    $('.meta[key="time_left"]').text(msg['state']);
    $('.meta[key="time_length"]').text("");
    $('.meta[key="time_pos"]').text("");
});

synapse_register("play_InfoMeta",function(msg_src, msg_dst, msg_event, msg)
{
    if (msg.title==null)
        msg.title="";
    $('.meta[key=title]').text(msg.title);

    if (msg.nowPlaying==null)
        msg.nowPlaying="";
    $('.meta[key=nowPlaying]').text(msg.nowPlaying);

});


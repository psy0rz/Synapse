/*  Copyright 2008,2009,2010 Edwin Eefting (edwin@datux.nl) 

    This file is part of Synapse.

    Synapse is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Synapse is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Synapse.  If not, see <http://www.gnu.org/licenses/>. */


#include "synapse.h"
#include "cconfig.h"
#include "clog.h"

namespace mp
{
    using namespace std;

    int playerId;

    SYNAPSE_REGISTER(module_Init)
    {
    	Cmsg out;

        playerId=0;

    	out.clear();
    	out.event="core_ChangeModule";
    	out["maxThreads"]=1;
    	out.send();

    	out.clear();
    	out.event="core_ChangeSession";
    	out["maxThreads"]=1;
    	out.send();

    	out.clear();
    	out.event="core_LoadModule";
        out["name"]="play_vlc";
        out.send();
    }


    SYNAPSE_REGISTER(play_vlc_Ready)
    {
        Cmsg out;
        out.event="core_LoadModule";
        out["name"]="pl_dir";
        out.send();
    }


    SYNAPSE_REGISTER(pl_dir_Ready)
    {
        Cmsg out;
        out.event="core_LoadModule";
        out["name"]="http_json";
        out.send();

        out.clear();
        out.event="pl_Create";
        out["id"]="/home/psy/pl";
        out.send();

    }

    SYNAPSE_REGISTER(http_json_Ready)
    {
        //tell the rest of the world we are ready for duty
        Cmsg out;
        out.clear();
        out.event="core_Ready";
        out.send();
    }

    // SYNAPSE_REGISTER(play_vlc_Ready)
    // {
    // 	playUrl="http://listen.di.fm/public3/chilloutdreams.pls";
    // 	Cmsg out;
    // 	out.dst=msg["session"];
    // 	out.event="play_NewPlayer";
    // 	out["description"]="second player instance";
    // 	out.send();
    // }

    //a new player has emerged
    SYNAPSE_REGISTER(play_Player)
    {
        //for now we just support one player
        if (!playerId)
            playerId=msg.src;
    }



    SYNAPSE_REGISTER(play_InfoMeta)
    {

    }



}

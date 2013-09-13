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


#include <boost/function_output_iterator.hpp>
#include <boost/bind.hpp>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iterator>
#include <iomanip>


namespace mp
{
    using namespace std;

    //int playerId;
    int plId;

    string wantFile;
    string openingFile;
    bool opening=false;

    //last received vlc state and file
    string vlcState;
    string vlcFile;

    //config file
    synapse::Cconfig config;
    CvarList playerIds;


    SYNAPSE_REGISTER(module_Init)
    {
    	Cmsg out;

        plId=0;

        config.load("etc/synapse/mp.conf");

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

        //create players
        FOREACH_VARLIST(player, config["players"])
        {
            Cmsg out;
            out.event="play_NewPlayer";
            out=player;
            out.send();
        }


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


    }

    SYNAPSE_REGISTER(http_json_Ready)
    {
        //tell the rest of the world we are ready for duty
        Cmsg out;
        out.clear();
        out.event="core_Ready";
        out.send();
    }


    //a new player has emerged
    SYNAPSE_REGISTER(play_Player)
    {
        playerIds.push_back(msg["id"]);

        //open the current wanted file right away
        Cmsg out;
        out.event="play_Open";
        out["ids"].list().push_back(msg["id"]);
        out["url"]=wantFile;
        out.send();
    }

    void sendOpen(string url)
    {
        Cmsg out;
        out.event="play_Open";
        out["ids"].list()=playerIds;
        out["url"]=url;
        out.send();
    }

    SYNAPSE_REGISTER(play_InfoMeta)
    {
        if (msg["id"]!="master")
            return;

        vlcFile=msg["url"].str();
    }

    SYNAPSE_REGISTER(play_State)
    {
        if (msg["id"]!="master")
            return;

        //static int lastTime=0;

        vlcState=msg["state"].str();

        if (msg["state"]=="empty")
        {
            /*
            if (time(NULL)-lastTime<=2)
            {
                sleep(1);
            }
            lastTime=time(NULL);
*/
            Cmsg out;
            out.dst=plId;
            out.event="pl_Next";
            out.send();
        };



        if (msg["state"]!="opening" && vlcFile==openingFile)
        {
            openingFile="";
            //we want a different file?
            if (vlcFile!=wantFile)
            {
                openingFile=wantFile;
                sendOpen(openingFile);
            }
        }
        
        
/*        if (msg["state"]=="playing")
        {
            Cmsg out;
            out.dst=playerId;
            out.event="play_SetTime";
            out["time"]=25;
            out.send();
            
        }*/
    }

    //url encode stuff, borrowed from http://stackoverflow.com/questions/3589936/c-urlencode-library-unicode-capable
    std::string encimpl(std::string::value_type v) {
        if (isalnum(v))
          return std::string()+v;

        std::ostringstream enc;
        enc << '%' << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << int(static_cast<unsigned char>(v));
        return enc.str();
    }


    //playlist switched to different path/file
    SYNAPSE_REGISTER(pl_Entry)
    {

        //for now we just support one playlist
        if (!plId)
            plId=msg.src;

        if (msg.src!=plId)
            return;

        string filename=msg["currentFile"].str();
        string encodedFilename;
        std::transform(filename.begin(), filename.end(),
            boost::make_function_output_iterator(boost::bind(static_cast<std::string& (std::string::*)(const std::string&)>(&std::string::append),&encodedFilename,_1)),
            encimpl);

        wantFile="file://"+encodedFilename;

        //not already opening stuff?
        if (openingFile=="")
        {
            //dont open it if vlc is already playing it
            if (wantFile!=vlcFile)
            {
                openingFile=wantFile;
                sendOpen(wantFile);
            }
        }
    }





}

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

/** \file
VLC player module

This module can play urls and local files.

*/

#include "synapse.h"
#include "cconfig.h"
#include "clog.h"
#include <vlc/vlc.h>
#include <map>
#include <string>

#include "exception/cexception.h"

#include "boost/bind.hpp"
#include <functional>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>

/** VLC player namespace
 *
 */
namespace play_vlc
{

using namespace std;
using namespace boost;


//one big mutex so that our threads and vlc threads dont collide
recursive_mutex vlcMutex;

//A player instance
//Consists of a libvlc player, list-player and and media-list (play queue)
//NOTE: the static vlc* functions are used for libvlc call backs. These are called from a seperate vlc thread!
class CPlayer
{
	private:
	int mId;

	//pointers to vlc objects and event managers
	libvlc_instance_t * mVlc;

    //dont log for now, since the log api doesnt even specify to which libvlc instance the logentries belong:
    //libvlc_log_subscriber_t  *mLogSubscriber;

	libvlc_media_list_t* mList;
	libvlc_event_manager_t *mListEm;

	libvlc_media_player_t* mPlayer;
	libvlc_event_manager_t *mPlayerEm;

	libvlc_media_list_player_t* mListPlayer;
	libvlc_event_manager_t *mListPlayerEm;

    int mVlcSubitemsLeft;

	int mVlcLastTime;
	string mVlcUrl;

    Cvar mStatus;

	public:
	string description;

    //send all cached status events to specified destination
    void sendStatus(int dst)
    {
        Cmsg out;
        out.src=mId;
        out.dst=dst;
        FOREACH_VARMAP(status, mStatus)
        {
            out.event=status.first;
            out=status.second;
            out.send();
        }

    }

	void throwError(string msg)
	{
		msg="[play_vlc] "+msg;

		if (libvlc_errmsg()!=NULL)
		{
			msg+=": ";
			msg+=libvlc_errmsg();
		}
		throw(synapse::runtime_error(msg));
	}


	//throw an error if the player-object is not ready to use
	void throwIfBad()
	{
		if (!ok())
			throwError("Player not initialized");
	}

	//throw an error if a vlc error occurred
	void throwIfVlcError()
	{
		if (libvlc_errmsg()!=NULL)
			throwError("error");
	}


	static void vlcEventGeneric(const libvlc_event_t * event, void *player)
	{
        lock_guard<recursive_mutex> lock(vlcMutex);

		Cmsg out;
		out.src=((CPlayer *)player)->mId;

		out.event=string("play_Event")+string(libvlc_event_type_name(event->type));
		out.send();
	}

	static void vlcEventMediaPlayerTimeChanged(const libvlc_event_t * event, void *player)
	{
        lock_guard<recursive_mutex> lock(vlcMutex);

		int newTime=event->u.media_player_time_changed.new_time/1000;

		//NOTE: this is called from another thread...watch out with accessing other variables
		if (newTime==((CPlayer *)player)->mVlcLastTime)
			return;

		((CPlayer *)player)->mVlcLastTime=newTime;

		Cmsg out;
		out.src=((CPlayer *)player)->mId;
		out.event="play_Time";
		out["time"]=newTime;
		out["length"]=(libvlc_media_player_get_length((libvlc_media_player_t*)event->p_obj))/1000;
		out.send();

        //cache status
        ((CPlayer *)player)->mStatus[out.event]=out;
	}

	//converts metadata from a mediaobject into a var
	static void vlcMeta2Var(libvlc_media_t * m, Cvar & var)
	{
		char * s;

		if ((s=libvlc_media_get_mrl(m)) != NULL)
			var["mrl"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Album)) != NULL)
			var["album"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Artist)) != NULL)
			var["artist"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_ArtworkURL)) != NULL)
			var["artworkUrl"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Copyright)) != NULL)
			var["copyright"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Date)) != NULL)
			var["date"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Description)) != NULL)
			var["description"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_EncodedBy)) != NULL)
			var["encodedBy"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Genre)) != NULL)
			var["genre"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Language)) != NULL)
			var["language"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_NowPlaying)) != NULL)
			var["nowPlaying"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Publisher)) != NULL)
			var["publisher"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Rating)) != NULL)
			var["rating"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Setting)) != NULL)
			var["setting"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_Title)) != NULL)
			var["title"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_TrackID)) != NULL)
			var["trackId"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_TrackNumber)) != NULL)
			var["trackNumber"]=string(s);

		if ((s=libvlc_media_get_meta(m, libvlc_meta_URL)) != NULL)
			var["url"]=string(s);

	}


	static void vlcEventMediaMetaChanged(const libvlc_event_t * event, void *player)
	{
        lock_guard<recursive_mutex> lock(vlcMutex);

		static Cmsg prevMsg;

		Cmsg out;
		out.src=((CPlayer *)player)->mId;
		out.event="play_InfoMeta";

		//its no use to look at event->u.media_meta_changed.meta_type, since that seems to give unrelated values.
		vlcMeta2Var((libvlc_media_t*)event->p_obj, out);

		//the original url the user has openend
		out["url"]=((CPlayer *)player)->mVlcUrl;

		if (out!=prevMsg)
		{
			out.send();
			prevMsg=out;

            //cache status
            ((CPlayer *)player)->mStatus[out.event]=out;

		}
	}



	static void vlcEventMediaStateChanged(const libvlc_event_t * event, void *player)
	{
        lock_guard<recursive_mutex> lock(vlcMutex);

/*
        DEB("lock");
        DEB("get");
        int playing=libvlc_media_player_is_playing((((CPlayer*)player)->mPlayer));
        ERROR("is player " << playing);
                DEB("unlock");

        DEB("unlocked");
*/
		Cmsg out;
		out.src=((CPlayer *)player)->mId;
        out.event="play_State";

        out["state"]="unknown";

		if (event->u.media_state_changed.new_state==libvlc_NothingSpecial)
            out["state"]="none";
		else if (event->u.media_state_changed.new_state==libvlc_Opening)
            out["state"]="opening";
		else if (event->u.media_state_changed.new_state==libvlc_Buffering)
	        out["state"]="buffering";
		else if (event->u.media_state_changed.new_state==libvlc_Playing)
            out["state"]="playing";
		else if (event->u.media_state_changed.new_state==libvlc_Paused)
            out["state"]="paused";
		else if (event->u.media_state_changed.new_state==libvlc_Stopped)
	        out["state"]="stopped";
		else if (event->u.media_state_changed.new_state==libvlc_Ended)
            out["state"]="ended";
		else if (event->u.media_state_changed.new_state==libvlc_Error)
	        out["state"]="error";

		out.send();

        ERROR(" LEFT " << ((CPlayer*)player)->mVlcSubitemsLeft);
        //are we done playing the internal playlists of vlc?        
        if (
            (event->u.media_state_changed.new_state==libvlc_Ended && ((CPlayer*)player)->mVlcSubitemsLeft==0) ||
            (event->u.media_state_changed.new_state==libvlc_Error)
        )
        {
            out["state"]="empty";
            out.send();
        }


        //cache status
        ((CPlayer *)player)->mStatus[out.event]=out;

        
/**
		//check the logs as well
		libvlc_log_message_t logMessage;
		out.clear();
		out.event="play_Log";

		//free the old iterator, and get a fresh one
		libvlc_log_iterator_free(((CPlayer *)player)->mLogIterator);
		((CPlayer *)player)->mLogIterator=libvlc_log_get_iterator(((CPlayer *)player)->mLog);
		while (libvlc_log_iterator_has_next(((CPlayer *)player)->mLogIterator))
		{

			libvlc_log_iterator_next(((CPlayer *)player)->mLogIterator, &logMessage);
			if (logMessage.i_severity==0)
				out["severity"]="info";
			else if (logMessage.i_severity==1)
				out["severity"]="error";
			else if (logMessage.i_severity==2)
				out["severity"]="warning";
			else if (logMessage.i_severity==3)
				out["severity"]="debug";

			out["type"]=logMessage.psz_type;
			out["name"]=logMessage.psz_name;
			out["message"]=logMessage.psz_message;
			if (logMessage.psz_header)
				out["header"]=logMessage.psz_header;
			out.send();
		}
		//clear logs
		libvlc_log_clear(((CPlayer *)player)->mLog);
**/
	}

    static void vlcEventMediaListPlayerNextItemSet(const libvlc_event_t * event, void *player)
    {
        lock_guard<recursive_mutex> lock(vlcMutex);
        ((CPlayer*)player)->mVlcSubitemsLeft--;
        ERROR(" LEFT " << ((CPlayer*)player)->mVlcSubitemsLeft);
    }


	static void vlcEventMediaSubItemAdded(const libvlc_event_t * event, void *player)
	{
        lock_guard<recursive_mutex> lock(vlcMutex);

        ((CPlayer*)player)->mVlcSubitemsLeft++;
        ERROR(" LEFT " << ((CPlayer*)player)->mVlcSubitemsLeft);
  
		libvlc_event_manager_t *em;
		em=libvlc_media_event_manager(event->u.media_subitem_added.new_child);
		if (em)
		{
			libvlc_event_attach(em, libvlc_MediaMetaChanged, vlcEventMediaMetaChanged, player);
			libvlc_event_attach(em, libvlc_MediaSubItemAdded, vlcEventMediaSubItemAdded, player);
			libvlc_event_attach(em, libvlc_MediaStateChanged, vlcEventMediaStateChanged, player);
		}

	}


	//create all the objects and attach all the event handlers
	CPlayer()
	{
		mVlc=NULL;
		mList=NULL;
		mPlayer=NULL;
		mListPlayer=NULL;
		libvlc_clearerr();
	}

	void init(int id)
	{
		this->mId=id;


		// Create vlc instance
		DEB("Creating vlc instance");
		mVlc=libvlc_new (0,NULL);
		if (!mVlc)
			throwError("Problem creating new vlc instance");

/*
		//open logger
		mLog=libvlc_log_open(mVlc);
		if (!mVlc)
			throwError("Problem opening vlc logger");

		mLogIterator=libvlc_log_get_iterator(mLog);
		if (!mLogIterator)
			throwError("Problem getting log iterator");
*/

		//create list player
		DEB("Creating list player");
		mListPlayer = libvlc_media_list_player_new(mVlc);
		if (!mListPlayer)
			throwError("Problem creating new list player");

		//get the event manager of the list player
		mListPlayerEm=libvlc_media_list_player_event_manager(mListPlayer);
		if (!mListPlayerEm)
			throwError("Problem getting event manager of list player");

       libvlc_event_attach(mListPlayerEm, libvlc_MediaListPlayerNextItemSet, vlcEventMediaListPlayerNextItemSet, this);

		//create a player
		DEB("Creating player");
		mPlayer=libvlc_media_player_new(mVlc);
		if (!mPlayer)
			throwError("Problem creating new player");

		//get the event manager of the player
		mPlayerEm=libvlc_media_player_event_manager(mPlayer);
		if (!mPlayerEm)
			throwError("Problem getting event manager of player");

        libvlc_event_attach(mPlayerEm, libvlc_MediaPlayerTimeChanged, vlcEventMediaPlayerTimeChanged, this);

		//create a list
		DEB("Creating list");
		mList=libvlc_media_list_new(mVlc);
		if (!mList)
			throwError("Problem creating new list");

		//get the event manager of the media list
		mListEm=libvlc_media_list_event_manager(mList);
		if (!mListEm)
			throwError("Problem getting event manager of list");

		libvlc_event_attach(mListEm, libvlc_MediaListItemAdded, vlcEventMediaSubItemAdded, this);


		//let the list player use this list
		libvlc_media_list_player_set_media_list(mListPlayer, mList);

		//let the list player use this player
		libvlc_media_list_player_set_media_player(mListPlayer, mPlayer);

	}

	bool ok()
	{
		return (/*mLogIterator!=NULL && mLog!=NULL &&*/ mVlc!=NULL && mPlayer!=NULL && mList!=NULL && mListPlayer!=NULL);
	}

	void destroy()
	{
		if (mPlayer)
		{

			stop();
			libvlc_media_player_release(mPlayer);
		}

		if (mList)
			libvlc_media_list_release(mList);

		if (mListPlayer)
			libvlc_media_list_player_release(mListPlayer);

/*
		if (mLogIterator)
			libvlc_log_iterator_free(mLogIterator);

		if (mLog)
			libvlc_log_close(mLog);
*/

		if (mVlc)
			libvlc_release(mVlc);

	}

	void open(string url)
	{
		throwIfBad();

		mVlcUrl=url;

		 // Create a new media item
		libvlc_media_t * m;
		m = libvlc_media_new_location(mVlc, url.c_str());

		if (!m)
			throwError("Cant creating location object");


		//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv LOCKED
		libvlc_media_list_lock(mList);
        DEB("remove locked");

		//clear the list
		while(libvlc_media_list_count(mList))
		{
            DEB("remove");
			libvlc_media_list_remove_index(mList, 0);
            DEB("remove done");
		}

		libvlc_media_list_unlock(mList);
		//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ LOCKED
        DEB("remove unlocked");

        mVlcSubitemsLeft=0;

        //vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv LOCKED
        libvlc_media_list_lock(mList);
        DEB("add locked");

        //add media
        libvlc_media_list_add_media(mList, m);
        libvlc_media_list_unlock(mList);
        //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ LOCKED
        DEB("add unlocked");


		//command the list player to play
        libvlc_media_list_player_next(mListPlayer);
		libvlc_media_list_player_play(mListPlayer);

		libvlc_media_release(m);

		throwIfVlcError();
	}

	void stop()
	{
		throwIfBad();
		libvlc_media_player_stop(mPlayer);
		throwIfVlcError();

		//make 100% sure it stopped, otherwise we get strange segfaults when exiting.
		//also we can get mutex problems with mVlc* variables.
		while(libvlc_media_player_is_playing(mPlayer))
		{
			DEB("Waiting for player to stop playing...");
			sleep(1);
		}
	}

    void pause()
    {
        libvlc_media_player_pause(mPlayer);
    }

    void setTime(int time)
    {
        libvlc_media_player_set_time(mPlayer, time);
    }

};

typedef map<int, CPlayer> CPlayerMap;
CPlayerMap players;
int defaultSession;



SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	defaultSession=dst;

	//this module is single threaded, since libvlc manages its own threads

	out.clear();
	out.event="core_Ready";
	out.send();

}

SYNAPSE_REGISTER(module_Shutdown)
{
}

SYNAPSE_REGISTER(module_SessionStart)
{
    lock_guard<recursive_mutex> lock(vlcMutex);

	players[dst].init(dst);

	//inform everyone there's a new player in town ;)
	Cmsg out;
	out=msg;
	out.event="play_Player";
	out.dst=0;
    out.src=msg.dst;
	out.send();
}

SYNAPSE_REGISTER(module_SessionEnd)
{
    lock_guard<recursive_mutex> lock(vlcMutex);

	players[dst].destroy();
	players.erase(dst);
}

/** Get a list of players
 * \REPLY play_Player
 *   For every player.
 *
 */
SYNAPSE_REGISTER(play_GetPlayers)
{
	Cmsg out;
	out.event="play_Player";
	out.dst=msg.src;

	for(CPlayerMap::iterator I=players.begin(); I!=players.end(); I++)
	{
        out.src=I->first;
        out.send();
	}
	out.send();
}

/** Delete the player instance. (You cant delete the default player)
 *
 */
SYNAPSE_REGISTER(play_DelPlayer)
{
	if (dst==defaultSession)
		throw(synapse::runtime_error("Cant delete default player"));

	Cmsg out;
	out.event="core_DelSession";
	out.send();
}

/** Delete the player instance. (You cant delete the default player)
 *
 */
SYNAPSE_REGISTER(play_NewPlayer)
{
	Cmsg out;
	out.event="core_NewSession";
	out["description"]=msg["description"];
	out["src"]=msg.src;
	out.send();
}

/** Opens and starts playing an url
    \P url The url or local filename to open.

\BROADCAST play_InfoMeta: 
    Metadata for the current url. (can also be sended while playing, for streams for example)

\BROADCAST play_State:
    Current status. 
    \param state:
        none:       no status yet
        opening:    Player is opening an url
        buffering:  Player is buffering data
        playing:    Player is playing something
        paused:     Player is pausing
        stopped:    Player is stopped
        ended:      Media has ended
        error:      an error occured. 
        TOOD: need to fix logging, to show what actually went wrong.    

\BROADCAST play_Time:
    Sended every second with time and position info.
        \arg \c length total length of media in seconds. (0 for streams)
        \arg \c time current position of the media, in seconds.

 */
SYNAPSE_REGISTER(play_Open)
{
    //lock_guard<recursive_mutex> lock(vlcMutex);

	INFO("vlc opening " << msg["url"].str());

	players[dst].open(msg["url"]);

}


/** Stops playing

*/
SYNAPSE_REGISTER(play_Stop)
{
    lock_guard<recursive_mutex> lock(vlcMutex);

	players[dst].stop();
}

SYNAPSE_REGISTER(play_Pause)
{
    lock_guard<recursive_mutex> lock(vlcMutex);

    players[dst].pause();
}


SYNAPSE_REGISTER(play_SetTime)
{
    lock_guard<recursive_mutex> lock(vlcMutex);

    players[dst].setTime(msg["time"] * 1000);
}

/** Gets status of player.
 * This is just to allow new clients to get the latest events. 
 *
 * Usually play_InfoMeta and stuff.
 *
 */
SYNAPSE_REGISTER(play_GetStatus)
{
    lock_guard<recursive_mutex> lock(vlcMutex);
    players[dst].sendStatus(msg.src);
}


//end namespace
}

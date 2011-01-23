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
#include <vlc/vlc.h>
#include <map>
#include <string>

#include "exception/cexception.h"

#include "boost/bind.hpp"
#include <functional>

namespace play_vlc
{

using namespace std;

libvlc_instance_t * vlcInst=NULL;

//A player instance
//Consists of a libvlc player, list-player and and media-list (play queue)
//NOTE: the static vlc* functions are used for libvlc call backs. These might be called from another parallel thread!
class CPlayer
{
	private:
	int mId;
	bool mHaveRef;

	//pointers to vlc objects and event managers
	libvlc_media_list_t* mList;
	libvlc_event_manager_t *mListEm;

	libvlc_media_player_t* mPlayer;
	libvlc_event_manager_t *mPlayerEm;

	libvlc_media_list_player_t* mListPlayer;
	libvlc_event_manager_t *mListPlayerEm;


	//WARNING: only access these variables from call back functions, or when player is stopped, to prevent mutex problems!
	int mVlcLastTime;
	string mVlcUrl;

	public:
	string description;

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

	bool ok()
	{
		return (mPlayer!=NULL && mList!=NULL && mListPlayer!=NULL);
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
		Cmsg out;
		out.src=((CPlayer *)player)->mId;
		out.event=string("play_Event")+string(libvlc_event_type_name(event->type));
		out.send();
	}

	static void vlcEventMediaPlayerTimeChanged(const libvlc_event_t * event, void *player)
	{
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
		}
	}

	static void vlcEventMediaStateChanged(const libvlc_event_t * event, void *player)
	{
		Cmsg out;
		out.src=((CPlayer *)player)->mId;

		if (event->u.media_state_changed.new_state==libvlc_NothingSpecial)
			out.event="play_StateNone";
		else if (event->u.media_state_changed.new_state==libvlc_Opening)
			out.event="play_StateOpening";
		else if (event->u.media_state_changed.new_state==libvlc_Buffering)
			out.event="play_StateBuffering";
		else if (event->u.media_state_changed.new_state==libvlc_Playing)
			out.event="play_StatePlaying";
		else if (event->u.media_state_changed.new_state==libvlc_Paused)
			out.event="play_StatePaused";
		else if (event->u.media_state_changed.new_state==libvlc_Stopped)
			out.event="play_StateStopped";
		else if (event->u.media_state_changed.new_state==libvlc_Ended)
			out.event="play_StateEnded";
		else if (event->u.media_state_changed.new_state==libvlc_Error)
			out.event="play_StateError";

		out.send();

	}

	static void vlcEventMediaSubItemAdded(const libvlc_event_t * event, void *player)
	{
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
		mHaveRef=false;
		mList=NULL;
		mPlayer=NULL;
		mListPlayer=NULL;
		libvlc_clearerr();
	}

	void init(int id)
	{
		this->mId=id;

		if (!vlcInst)
			throwError("No vlc instance found");

		//increase vlc reference count
		DEB("Increasing reference count");
		libvlc_retain(vlcInst);
		mHaveRef=true;

		//create list player
		DEB("Creating list player");
		mListPlayer = libvlc_media_list_player_new(vlcInst);
		if (!mListPlayer)
			throwError("Problem creating new list player");

		//get the event manager of the list player
		mListPlayerEm=libvlc_media_list_player_event_manager(mListPlayer);
		if (!mListPlayerEm)
			throwError("Problem getting event manager of list player");



		//create a player
		DEB("Creating player");
		mPlayer=libvlc_media_player_new(vlcInst);
		if (!mPlayer)
			throwError("Problem creating new player");

		//get the event manager of the player
		mPlayerEm=libvlc_media_player_event_manager(mPlayer);
		if (!mPlayerEm)
			throwError("Problem getting event manager of player");

		libvlc_event_attach(mPlayerEm, libvlc_MediaPlayerTimeChanged, vlcEventMediaPlayerTimeChanged, this);


		//create a list
		DEB("Creating list");
		mList=libvlc_media_list_new(vlcInst);
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

	void open(string url)
	{
		throwIfBad();

		stop();

		mVlcUrl=url;

		 // Create a new media item
		libvlc_media_t * m;
		m = libvlc_media_new_location(vlcInst, url.c_str());

		if (!m)
			throwError("Cant creating location object");

		//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv LOCKED
		libvlc_media_list_lock(mList);

		//clear the list
		while(libvlc_media_list_count(mList))
		{
			libvlc_media_list_remove_index(mList, 0);
		}

		//add media
		libvlc_media_list_add_media(mList, m);

		libvlc_media_list_unlock(mList);
		//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ LOCKED

		//command the list player to play
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

		if (mHaveRef && vlcInst)
			libvlc_release(vlcInst);

		mHaveRef=false;
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

	// Load the VLC engine
	DEB("Loading vlc engine");

	vlcInst=libvlc_new (0,NULL);

	if (vlcInst)
	{
		out.clear();
		out.event="core_Ready";
		out.send();
	}
	else
	{
		throw(synapse::runtime_error("VLC initalisation failed"));
	}
}

SYNAPSE_REGISTER(module_Shutdown)
{
	//the shutdown comes before all the sessionends, but this is no problem because of vlc's reference counting.
	//decrease vlc reference count
	if (vlcInst)
	{
		libvlc_release(vlcInst);
	}
}

SYNAPSE_REGISTER(module_SessionStart)
{
	players[dst].init(dst);

	//inform everyone there's a new player in town ;)
	Cmsg out;
	out=msg;
	out.event="play_Player";
	out.src=dst;
	out.dst=0;
	out.send();
}

SYNAPSE_REGISTER(module_SessionEnd)
{
	players[dst].destroy();
	players.erase(msg.dst);
}

/** Get a list of players
 * Returns a play_Players event with a list of available player ids
 *
 */
SYNAPSE_REGISTER(play_GetPlayers)
{
	Cmsg out;
	out.event="play_Players";
	out.dst=msg.src;

	for(CPlayerMap::iterator I=players.begin(); I!=players.end(); I++)
	{
		stringstream s;
		s << I->first;
		out["players"][s.str()]=I->second.description;
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
	out.src=dst;
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

/** Open the specified url
 *
 */
SYNAPSE_REGISTER(play_Open)
{
	INFO("vlc opening " << msg["url"].str());

	players[dst].open(msg["url"]);

}


SYNAPSE_REGISTER(play_Stop)
{
	players[msg.dst].stop();
}




//end namespace
}

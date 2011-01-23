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
class CPlayer
{
	private:
	public:
	//vlc objects and event managers
	libvlc_media_list_t* vlcList;
	libvlc_event_manager_t *vlcListEm;

	libvlc_media_player_t* vlcPlayer;
	libvlc_event_manager_t *vlcPlayerEm;

	libvlc_media_list_player_t* vlcListPlayer;
	libvlc_event_manager_t *vlcListPlayerEm;


	public:

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
		return (vlcPlayer!=NULL && vlcList!=NULL && vlcListPlayer!=NULL);
	}

	//throw an error if the player-object is not ready to use
	void throwIfBad()
	{
		if (!ok())
			throwError("Player not ready");
	}

	//throw an error if a vlc error occured
	void throwIfVlcError()
	{
		if (libvlc_errmsg()!=NULL)
			throwError("error");
	}


	static void vlcEventMediaListPlayerNextItemSet(const libvlc_event_t * event, void *player)
	{

	//	vlcPlayer=NULL;
		INFO("Adding meta handler");
//		libvlc_event_manager_t *em;
//		em=libvlc_media_event_manager(event->u.media_list_player_next_item_set.item);
//		if (em)
//		{
//			libvlc_event_attach(em, libvlc_MediaMetaChanged, vlcEventMediaMetaChanged, sessionId);
//			libvlc_event_attach(em, libvlc_MediaSubItemAdded, vlcEventGeneric, sessionId);
//			libvlc_event_attach(em, libvlc_MediaStateChanged, vlcEventMediaStateChanged, sessionId);
//		}

	}


	//create all the objects and attach all the event handlers
	CPlayer()
	{
		vlcList=NULL;
		vlcPlayer=NULL;
		vlcListPlayer=NULL;
		libvlc_clearerr();

		if (!vlcInst)
			throwError("No vlc instance found");

		//increase vlc reference count
		libvlc_retain(vlcInst);


		//create list player
		vlcListPlayer = libvlc_media_list_player_new(vlcInst);
		if (!vlcListPlayer)
			throwError("Problem creating new list player");

		//get the event manager of the list player
		vlcListPlayerEm=libvlc_media_list_player_event_manager(vlcListPlayer);
		if (!vlcListPlayerEm)
			throwError("Problem getting event manager of list player");

		//libvlc_event_attach(em, libvlc_MediaListPlayerStopped, vlcEventGeneric, (void*)msg.dst);
		//libvlc_event_attach(em, libvlc_MediaListPlayerPlayed, vlcEventGeneric, (void*)msg.dst);


		libvlc_event_attach(
				vlcListPlayerEm, libvlc_MediaListPlayerNextItemSet,
				vlcEventMediaListPlayerNextItemSet,
				this);


		//create a player
		vlcPlayer=libvlc_media_player_new(vlcInst);
		if (!vlcPlayer)
			throwError("Problem creating new player");

		//get the event manager of the player
		vlcPlayerEm=libvlc_media_player_event_manager(vlcPlayer);
		if (!vlcPlayerEm)
			throwError("Problem getting event manager of player");

		//attach player event handlers
//			libvlc_event_attach(em, libvlc_MediaPlayerOpening, vlcEventMediaPlayerOpening, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaPlayerBuffering, vlcEventMediaPlayerBuffering, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaPlayerPlaying, vlcEventMediaPlayerPlaying, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaPlayerPaused, vlcEventMediaPlayerPaused, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaPlayerStopped, vlcEventMediaPlayerStopped, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaPlayerEndReached, vlcEventMediaPlayerEndReached, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError,	vlcEventMediaPlayerEncounteredError, (void*)msg.dst);

////		libvlc_event_attach(vlcPlayerEm, libvlc_MediaPlayerTimeChanged, vlcEventMediaPlayerTimeChanged, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaPlayerPositionChanged, vlcEventMediaPlayerPositionChanged, (void*)msg.dst);

//jittert			libvlc_event_attach(em, libvlc_MediaPlayerLengthChanged, vlcEventGeneric, (void*)msg.dst);
////		libvlc_event_attach(vlcPlayerEm, libvlc_MediaPlayerTitleChanged, vlcEventMediaPlayerTitleChanged, (void*)msg.dst);


		//create a list
		vlcList=libvlc_media_list_new(vlcInst);
		if (!vlcList)
			throwError("Problem creating new list");

		//get the event manager of the media list
		vlcListEm=libvlc_media_list_event_manager(vlcList);
		if (!vlcListEm)
			throwError("Problem getting event manager of list");

////		libvlc_event_attach(em, libvlc_MediaListItemDeleted, vlcEventGeneric, (void*)msg.dst);
////		libvlc_event_attach(em, libvlc_MediaListItemAdded, vlcEventGeneric, (void*)msg.dst);


		//let the list player use this list
		libvlc_media_list_player_set_media_list(vlcListPlayer, vlcList);

		//let the list player use this player
		libvlc_media_list_player_set_media_player(vlcListPlayer, vlcPlayer);

	}

	void open(string url)
	{
		throwIfBad();

		 // Create a new media item
		libvlc_media_t * m;
		m = libvlc_media_new_location(vlcInst, url.c_str());

		if (!m)
			throwError("Cant creating location object");

		//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv LOCKED
		libvlc_media_list_lock(vlcList);

		//clear the list
		while(libvlc_media_list_count(vlcList))
		{
			libvlc_media_list_remove_index(vlcList, 0);
		}

		//add media
		libvlc_media_list_add_media(vlcList, m);

		libvlc_media_list_unlock(vlcList);
		//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ LOCKED

		//command the list player to play
		libvlc_media_list_player_play(vlcListPlayer);

		//niet hier ivm subitems!
//		libvlc_event_manager_t *em;
//		em=libvlc_media_event_manager(m);
//		if (em)
//		{
//			libvlc_event_attach(em, libvlc_MediaMetaChanged, vlcEventMediaMetaChanged, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaSubItemAdded, vlcEventMediaSubItemAdded, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaStateChanged, vlcEventGeneric, (void*)msg.dst);
//		}

		libvlc_media_release(m);

		throwIfVlcError();
	}

	void stop()
	{
		throwIfBad();
		libvlc_media_player_stop(vlcPlayer);
		throwIfVlcError();
	}

	void clean()
	{
		if (vlcPlayer)
		{
			libvlc_media_player_stop(vlcPlayer);

			//make 100% sure it stopped, otherwise we get strange segfaults and stuff
			while(libvlc_media_player_is_playing(vlcPlayer))
			{
				DEB("Waiting for player to stop playing...");
				sleep(1);
			}
			libvlc_media_player_release(vlcPlayer);
		}

		if (vlcList)
			libvlc_media_list_release(vlcList);

		if (vlcListPlayer)
			libvlc_media_list_player_release(vlcListPlayer);

		if (vlcInst)
			libvlc_release(vlcInst);
	}
};

map<int, CPlayer> players;


//void vlcEventMediaPlayerOpening(const libvlc_event_t * event, void *sessionId)
//{
//	Cmsg out;
//	out.src=(long int)sessionId;
//	out.event="play_StateOpening";
//	out.send();
//
//}
//
//void vlcEventMediaPlayerBuffering(const libvlc_event_t * event, void *sessionId)
//{
//	Cmsg out;
//	out.src=(long int)sessionId;
//	out.event="play_StateBuffering";
//	out.send();
//}
//
//void vlcEventMediaPlayerPlaying(const libvlc_event_t * event, void *sessionId)
//{
//	Cmsg out;
//	out.src=(long int)sessionId;
//	out.event="play_StatePlaying";
//	out.send();
//}
//
//void vlcEventMediaPlayerPaused(const libvlc_event_t * event, void *sessionId)
//{
//	Cmsg out;
//	out.src=(long int)sessionId;
//	out.event="play_StatePaused";
//	out.send();
//}
//
//void vlcEventMediaPlayerStopped(const libvlc_event_t * event, void *sessionId)
//{
//	Cmsg out;
//	out.src=(long int)sessionId;
//	out.event="play_StateStopped";
//	out.send();
//}
//
//void vlcEventMediaPlayerEndReached(const libvlc_event_t * event, void *sessionId)
//{
//	Cmsg out;
//	out.src=(long int)sessionId;
//	out.event="play_StateEnded";
//	out.send();
//}
//
//void vlcEventMediaPlayerEncounteredError(const libvlc_event_t * event, void *sessionId)
//{
//	Cmsg out;
//	out.src=(long int)sessionId;
//	out.event="play_StateError";
//	out.send();
//}

void vlcEventGeneric(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event=string("play_Event")+string(libvlc_event_type_name(event->type));
	out.send();
}

void vlcEventMediaPlayerTimeChanged(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_Time";
	out["time"]=event->u.media_player_time_changed.new_time;
	out["length"]=libvlc_media_player_get_length((libvlc_media_player_t*)event->p_obj);
	out.send();
}

void vlcEventMediaPlayerTitleChanged(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_EventTitle";
	out["title"]=libvlc_media_player_get_title((libvlc_media_player_t*)event->p_obj);
	out.send();
}


//converts metadata from a mediaobject into a message
void vlcMeta2Msg(libvlc_media_t * m, Cmsg & msg)
{
	char * s;

	if ((s=libvlc_media_get_mrl(m)) != NULL)
		msg["mrl"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Album)) != NULL)
		msg["album"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Artist)) != NULL)
		msg["artist"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_ArtworkURL)) != NULL)
		msg["artworkUrl"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Copyright)) != NULL)
		msg["copyright"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Date)) != NULL)
		msg["date"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Description)) != NULL)
		msg["description"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_EncodedBy)) != NULL)
		msg["encodedBy"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Genre)) != NULL)
		msg["genre"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Language)) != NULL)
		msg["language"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_NowPlaying)) != NULL)
		msg["nowPlaying"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Publisher)) != NULL)
		msg["publisher"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Rating)) != NULL)
		msg["rating"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Setting)) != NULL)
		msg["setting"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_Title)) != NULL)
		msg["title"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_TrackID)) != NULL)
		msg["trackId"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_TrackNumber)) != NULL)
		msg["trackNumber"]=string(s);

	if ((s=libvlc_media_get_meta(m, libvlc_meta_URL)) != NULL)
		msg["url"]=string(s);


}




void vlcEventMediaMetaChanged(const libvlc_event_t * event, void *sessionId)
{
	static Cmsg prevMsg;

	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_InfoMeta";

	//its no use to look at event->u.media_meta_changed.meta_type, since that seems to give unrelated values.
	vlcMeta2Msg((libvlc_media_t*)event->p_obj, out);

	if (out!=prevMsg)
	{
		out.send();
		prevMsg=out;
	}
}

void vlcEventMediaStateChanged(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;

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
void vlcEventMediaSubItemAdded(const libvlc_event_t * event, void *sessionId)
{
//	INFO("Adding meta handler");
//	libvlc_event_manager_t *em;
//	em=libvlc_media_event_manager(event->u.media_subitem_added.new_child);
//	if (em)
//	{
//		libvlc_event_attach(em, libvlc_MediaMetaChanged, vlcEventMediaMetaChanged, sessionId);
//		libvlc_event_attach(em, libvlc_MediaSubItemAdded, vlcEventMediaSubItemAdded, sessionId);
//		libvlc_event_attach(em, libvlc_MediaStateChanged, vlcEventGeneric, sessionId);
//	}

}





//void vlcEventMediaPlayerPositionChanged(const libvlc_event_t * event, void *sessionId)
//{
//	Cmsg out;
//	out.src=(long int)sessionId;
//	out.event="play_Time";
//	out["position"]=event->u.media_player_position_changed.new_position;
//	out.send();
//}


SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	//this module is single threaded, since libvlc manages its own threads
//	out.clear();
//	out.event="core_ChangeModule";
//	out["maxThreads"]=1;
//	out.send();
//
//	out.clear();
//	out.event="core_ChangeSession";
//	out["maxThreads"]=1;
//	out.send();

	// Load the VLC engine
	DEB("Loading vlc engine");

//	char p[200];
//	const char *pp=p;
//	strcpy(p,"-vvvv");
//	vlcInst=libvlc_new (1, &pp);
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
	libvlc_clearerr();

	//every session has its own player
	//so we CAN, if we want, create multiple instances of the player.
//	if (players[msg.dst].ok())
//	{

	if (libvlc_errmsg())
	{
		ERROR("Error while creating new vlc player: " << libvlc_errmsg());
		//something went wrong, delete session/player
		Cmsg out;
		out.event="core_DelSession";
		out.src=msg.dst;
		out.send();
	}
	else
	{
		//tell everyone this player is ready to be used
		Cmsg out;
		out.event="play_Ready";
		out.src=msg.dst;
		out.send();
	}
}

SYNAPSE_REGISTER(module_SessionEnd)
{
	players[dst].clean();
	players.erase(msg.dst);
}

/** Open the specified url
 *  TODO: implement functions to manage a play-queue. (basics have been setup for this)
 *
 */
SYNAPSE_REGISTER(play_Open)
{
	libvlc_clearerr();

	INFO("vlc opening " << msg["url"].str());

	players[dst].open(msg["url"]);

}



SYNAPSE_REGISTER(play_Stop)
{
	players[msg.dst].stop();
}




}

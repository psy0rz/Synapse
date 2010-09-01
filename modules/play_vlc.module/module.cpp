#include "synapse.h"
#include "cconfig.h"
#include "clog.h"
#include <vlc/vlc.h>
#include <map>
#include <string>



namespace play_vlc
{

using namespace std;

libvlc_instance_t * vlcInst=NULL;

//A player instance
//Consists of a libvlc player, list-player and and media-list (play queue)
class CPlayer
{
	public:

	libvlc_media_list_t* vlcList;
	libvlc_media_player_t* vlcPlayer;
	libvlc_media_list_player_t* vlcListPlayer;

	//create all the objects
	CPlayer()
	{
		vlcList=NULL;
		vlcPlayer=NULL;
		vlcListPlayer=NULL;

		if (vlcInst)
		{
			//increase vlc reference count
			libvlc_retain(vlcInst);

			//create list player
			vlcListPlayer = libvlc_media_list_player_new(vlcInst);
			if (vlcListPlayer)
			{
				//create a player
				vlcPlayer=libvlc_media_player_new(vlcInst);

				if (vlcPlayer)
				{
					//create a list
					vlcList=libvlc_media_list_new(vlcInst);

					if (vlcList)
					{
						//let the list player use this list
						libvlc_media_list_player_set_media_list(vlcListPlayer, vlcList);

						//let the list player use this player
						libvlc_media_list_player_set_media_player(vlcListPlayer, vlcPlayer);
					}
				}
			}
		}
	}

	bool ok()
	{
		return (vlcPlayer!=NULL && vlcList!=NULL && vlcListPlayer!=NULL);
	}


	void clean()
	{
		if (vlcPlayer)
			libvlc_media_player_release(vlcPlayer);

		if (vlcList)
			libvlc_media_list_release(vlcList);

		if (vlcListPlayer)
			libvlc_media_list_player_release(vlcListPlayer);

		if (vlcInst)
			libvlc_release(vlcInst);
	}
};

map<int, CPlayer> players;


////////////////////////////// VLC event callbacks
/*
 * libvlc_NothingSpecial
libvlc_Opening
libvlc_Buffering
libvlc_Playing
libvlc_Paused
libvlc_Stopped
libvlc_Ended
libvlc_Error

vlc_MediaPlayerOpening
libvlc_MediaPlayerBuffering
libvlc_MediaPlayerPlaying
libvlc_MediaPlayerPaused
libvlc_MediaPlayerStopped
libvlc_MediaPlayerEndReached
libvlc_MediaPlayerEncounteredError
 *
 */

void vlcEventMediaPlayerOpening(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_StateOpening";
	out.send();

}

void vlcEventMediaPlayerBuffering(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_StateBuffering";
	out.send();
}

void vlcEventMediaPlayerPlaying(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_StatePlaying";
	out.send();
}

void vlcEventMediaPlayerPaused(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_StatePaused";
	out.send();
}

void vlcEventMediaPlayerStopped(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_StateStopped";
	out.send();
}

void vlcEventMediaPlayerEndReached(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_StateEnded";
	out.send();
}

void vlcEventMediaPlayerEncounteredError(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_StateError";
	out.send();
}

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
	out.event="play_EventTime";
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

void vlcEventMediaMetaChanged(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(long int)sessionId;
	out.event="play_EventMeta";

	libvlc_media_parse((libvlc_media_t*)event->p_obj);

	char * s;

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, event->u.media_meta_changed.meta_type)) != NULL)
		out["changed"]=string(s);


	INFO("changed=" << event->u.media_meta_changed.meta_type);

//	if ((s=libvlc_media_get_mrl((libvlc_media_t*)event->p_obj)) != NULL)
//		out["mrl"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Album)) != NULL)
		out["album"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Artist)) != NULL)
		out["artist"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_ArtworkURL)) != NULL)
		out["artworkUrl"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Copyright)) != NULL)
		out["copyright"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Date)) != NULL)
		out["date"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Description)) != NULL)
		out["description"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_EncodedBy)) != NULL)
		out["encodedBy"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Genre)) != NULL)
		out["genre"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Language)) != NULL)
		out["language"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_NowPlaying)) != NULL)
		out["nowPlaying"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Publisher)) != NULL)
		out["publisher"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Rating)) != NULL)
		out["rating"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Setting)) != NULL)
		out["setting"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_Title)) != NULL)
		out["title"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_TrackID)) != NULL)
		out["trackId"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_TrackNumber)) != NULL)
		out["trackNumber"]=string(s);

	if ((s=libvlc_media_get_meta((libvlc_media_t*)event->p_obj, libvlc_meta_URL)) != NULL)
		out["url"]=string(s);

	out.send();
}

void vlcEventMediaListPlayerNextItemSet(const libvlc_event_t * event, void *sessionId)
{
	INFO("Adding meta handler");
	libvlc_event_manager_t *em;
	em=libvlc_media_event_manager(event->u.media_list_player_next_item_set.item);
	if (em)
	{
		libvlc_event_attach(em, libvlc_MediaMetaChanged, vlcEventMediaMetaChanged, sessionId);
		libvlc_event_attach(em, libvlc_MediaSubItemAdded, vlcEventGeneric, sessionId);
		libvlc_event_attach(em, libvlc_MediaStateChanged, vlcEventGeneric, sessionId);
	}
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
	vlcInst=libvlc_new (0, NULL);
	if (vlcInst)
	{
		out.clear();
		out.event="core_Ready";
		out.send();
	}
	else
	{
		//TODO
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
	if (players[msg.dst].ok())
	{
		libvlc_event_manager_t *em;

		//get the event manager of the player
		em=libvlc_media_player_event_manager(players[msg.dst].vlcPlayer);
		if (em)
		{
			//attach our event handlers
			libvlc_event_attach(em, libvlc_MediaPlayerOpening, vlcEventMediaPlayerOpening, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaPlayerBuffering, vlcEventMediaPlayerBuffering, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaPlayerPlaying, vlcEventMediaPlayerPlaying, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaPlayerPaused, vlcEventMediaPlayerPaused, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaPlayerStopped, vlcEventMediaPlayerStopped, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaPlayerEndReached, vlcEventMediaPlayerEndReached, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError,	vlcEventMediaPlayerEncounteredError, (void*)msg.dst);

//			libvlc_event_attach(em, libvlc_MediaPlayerTimeChanged, vlcEventMediaPlayerTimeChanged, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaPlayerPositionChanged, vlcEventMediaPlayerPositionChanged, (void*)msg.dst);

//jittert			libvlc_event_attach(em, libvlc_MediaPlayerLengthChanged, vlcEventGeneric, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaPlayerTitleChanged, vlcEventMediaPlayerTitleChanged, (void*)msg.dst);

		}

		//get the event manager of the media list player
		em=libvlc_media_list_player_event_manager(players[msg.dst].vlcListPlayer);
		if (em)
		{
			libvlc_event_attach(em, libvlc_MediaListPlayerNextItemSet, vlcEventMediaListPlayerNextItemSet, (void*)msg.dst);
		}

		//get the event manager of the media list
		em=libvlc_media_list_event_manager(players[msg.dst].vlcList);
		if (em)
		{
			libvlc_event_attach(em, libvlc_MediaListPlayerStopped, vlcEventGeneric, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaListPlayerPlayed, vlcEventGeneric, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaListItemDeleted, vlcEventGeneric, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaListItemAdded, vlcEventGeneric, (void*)msg.dst);
		}


	}

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
	players[msg.dst].clean();
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

	if (players[msg.dst].ok())
	{
		 // Create a new media item
		libvlc_media_t * m;
		m = libvlc_media_new_location(vlcInst, msg["url"].str().c_str());

		//niet hier ivm subitems!
//		libvlc_event_manager_t *em;
//		em=libvlc_media_event_manager(m);
//		if (em)
//		{
//			libvlc_event_attach(em, libvlc_MediaMetaChanged, vlcEventMediaMetaChanged, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaSubItemAdded, vlcEventGeneric, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaStateChanged, vlcEventGeneric, (void*)msg.dst);
//		}


		if (m)
		{
			libvlc_media_list_lock(players[msg.dst].vlcList);

			//clear the list
			while(libvlc_media_list_count(players[msg.dst].vlcList))
			{
				libvlc_media_list_remove_index(players[msg.dst].vlcList, 0);
			}

			//add media
			libvlc_media_list_add_media(players[msg.dst].vlcList, m);
			libvlc_media_list_unlock(players[msg.dst].vlcList);

			//command the list player to play
			libvlc_media_list_player_play(players[msg.dst].vlcListPlayer);

			libvlc_media_release(m);
		}
	}


	if (libvlc_errmsg())
	{
		msg.returnError(libvlc_errmsg());
	}
}







//{
//     /* Create a new item */
//     m = libvlc_media_new_location (inst, "/mnt/server/.mldonkey/incoming/Deep Rising KLAXXON.avi");
//
//     /* Create a media player playing environement */
//     mp = libvlc_media_player_new_from_media (m);
//
//     /* No need to keep the media now */
//     libvlc_media_release (m);
//
// #if 0
//     /* This is a non working code that show how to hooks into a window,
//      * if we have a window around */
//      libvlc_media_player_set_xdrawable (mp, xdrawable);
//     /* or on windows */
//      libvlc_media_player_set_hwnd (mp, hwnd);
//     /* or on mac os */
//      libvlc_media_player_set_nsobject (mp, view);
//  #endif
//
//     /* play the media_player */
//     libvlc_media_player_play (mp);
//
//     sleep (10); /* Let it play a bit */
//
//     /* Stop playing */
//     libvlc_media_player_stop (mp);
//
//     /* Free the media_player */
//     libvlc_media_player_release (mp);
//
//     libvlc_release (inst);
//
//
// 	///tell the rest of the world we are ready for duty
// 	out.clear();
// 	out.event="core_Ready";
// 	out.send();
//
//}
//
//

}

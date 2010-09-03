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
	out.event="play_EventMeta";

	//its no use to look at event->u.media_meta_changed.meta_type, since that seems to give unrelated values.
	vlcMeta2Msg((libvlc_media_t*)event->p_obj, out);

	if (out!=prevMsg)
	{
		out.send();
		prevMsg=out;
	}
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

			libvlc_event_attach(em, libvlc_MediaPlayerTimeChanged, vlcEventMediaPlayerTimeChanged, (void*)msg.dst);
//			libvlc_event_attach(em, libvlc_MediaPlayerPositionChanged, vlcEventMediaPlayerPositionChanged, (void*)msg.dst);

//jittert			libvlc_event_attach(em, libvlc_MediaPlayerLengthChanged, vlcEventGeneric, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaPlayerTitleChanged, vlcEventMediaPlayerTitleChanged, (void*)msg.dst);

		}

		//get the event manager of the media list player
		em=libvlc_media_list_player_event_manager(players[msg.dst].vlcListPlayer);
		if (em)
		{
			//libvlc_event_attach(em, libvlc_MediaListPlayerStopped, vlcEventGeneric, (void*)msg.dst);
			//libvlc_event_attach(em, libvlc_MediaListPlayerPlayed, vlcEventGeneric, (void*)msg.dst);
			libvlc_event_attach(em, libvlc_MediaListPlayerNextItemSet, vlcEventMediaListPlayerNextItemSet, (void*)msg.dst);
		}

		//get the event manager of the media list
		em=libvlc_media_list_event_manager(players[msg.dst].vlcList);
		if (em)
		{
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
//		m = libvlc_media_new_path(vlcInst, msg["url"].str().c_str());

		if (m)
		{
//			Cmsg out;
//			out.event="play_Meta";
//			libvlc_media_parse(m);
//			vlcMeta2Msg(m, out);
//			out.send();


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

			//niet hier ivm subitems!
			libvlc_event_manager_t *em;
			em=libvlc_media_event_manager(m);
			if (em)
			{
				libvlc_event_attach(em, libvlc_MediaMetaChanged, vlcEventMediaMetaChanged, (void*)msg.dst);
				libvlc_event_attach(em, libvlc_MediaSubItemAdded, vlcEventMediaSubItemAdded, (void*)msg.dst);
				libvlc_event_attach(em, libvlc_MediaStateChanged, vlcEventGeneric, (void*)msg.dst);
			}


			libvlc_media_release(m);
		}
	}


	if (libvlc_errmsg())
	{
		msg.returnError(libvlc_errmsg());
	}
}







}

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
map<int,libvlc_media_player_t*> vlcPlayers;

//gets specified player pointer or throws exception
//Watch out for vlc memleaks!
libvlc_media_player_t * getPlayer(int id)
{
	if (vlcPlayers.find(id)==vlcPlayers.end())
	{
		throw(runtime_error("Player not found"));
	}
	else
	{
		return(vlcPlayers[id]);
	}
}


//throws an exception if a vlc error occured
//Watch out for vlc memleaks!
void vlcErrorCheck()
{
	if (libvlc_errmsg() != NULL)
	{
		string s(libvlc_errmsg());
		libvlc_clearerr();
		throw(runtime_error(s));
	}
}

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
	out.src=(int)sessionId;
	out.event="play_StateOpening";
	out.send();
}

void vlcEventMediaPlayerBuffering(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(int)sessionId;
	out.event="play_StateBuffering";
	out.send();
}

void vlcEventMediaPlayerPlaying(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(int)sessionId;
	out.event="play_StatePlaying";
	out.send();
}

void vlcEventMediaPlayerPaused(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(int)sessionId;
	out.event="play_StatePaused";
	out.send();
}

void vlcEventMediaPlayerStopped(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(int)sessionId;
	out.event="play_StateStopped";
	out.send();
}

void vlcEventMediaPlayerEndReached(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(int)sessionId;
	out.event="play_StateEnded";
	out.send();
}

void vlcEventMediaPlayerEncounteredError(const libvlc_event_t * event, void *sessionId)
{
	Cmsg out;
	out.src=(int)sessionId;
	out.event="play_StateEnded";
	out.send();
}


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
	vlcErrorCheck();

  	out.clear();
  	out.event="core_Ready";
  	out.send();
}

SYNAPSE_REGISTER(module_Shutdown)
{
	//the shutdown comes before all the sessionends, but this is no problem because of vlc's reference counting.
	//decrease vlc reference count
	if (vlcInst)
	{
		libvlc_release(vlcInst);
		vlcErrorCheck();
	}
}

SYNAPSE_REGISTER(module_SessionStart)
{
	if (vlcInst)
	{
		//increase vlc reference count
		libvlc_retain(vlcInst);
		vlcErrorCheck();

		//every session has its own player
		//so we CAN, if we want, create multiple instances of the player.
		vlcPlayers[msg.dst] = libvlc_media_player_new(vlcInst);
		vlcErrorCheck();

		//get the event manager of this player
		libvlc_event_manager_t *em=libvlc_media_player_event_manager(vlcPlayers[msg.dst]);
		vlcErrorCheck();

		//attach our event handlers
		libvlc_event_attach(em, libvlc_MediaPlayerOpening, vlcEventMediaPlayerOpening, (void*)msg.dst);
		libvlc_event_attach(em, libvlc_MediaPlayerBuffering, vlcEventMediaPlayerBuffering, (void*)msg.dst);
		libvlc_event_attach(em, libvlc_MediaPlayerPlaying, vlcEventMediaPlayerPlaying, (void*)msg.dst);
		libvlc_event_attach(em, libvlc_MediaPlayerPaused, vlcEventMediaPlayerPaused, (void*)msg.dst);
		libvlc_event_attach(em, libvlc_MediaPlayerStopped, vlcEventMediaPlayerStopped, (void*)msg.dst);
		libvlc_event_attach(em, libvlc_MediaPlayerEndReached, vlcEventMediaPlayerEndReached, (void*)msg.dst);
		libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError,	vlcEventMediaPlayerEncounteredError, (void*)msg.dst);
		vlcErrorCheck();

		//tell everyone this player is ready to be used
		Cmsg out;
		out.event="play_Ready";
		out.src=msg.dst;
		out.send();
	}

}

SYNAPSE_REGISTER(module_SessionEnd)
{
	if (vlcInst)
	{
		if (vlcPlayers.find(msg.dst)!=vlcPlayers.end())
		{
			//release player
			libvlc_media_player_release(vlcPlayers[msg.dst]);
		}

		//decrease vlc reference count
		libvlc_release(vlcInst);
	}
}


SYNAPSE_REGISTER(play_Open)
{
	getPlayer(msg.dst);

	 // Create a new media item
	libvlc_media_t * m;
	m = libvlc_media_new_location (vlcInst, msg["url"].str().c_str());
	vlcErrorCheck();

	// Let player open it
	libvlc_media_player_set_media(getPlayer(msg.dst), m);

	// we dont need to keep it
	libvlc_media_release(m);

	// start playing it
	libvlc_media_player_play(vlcPlayers[msg.dst]);
	vlcErrorCheck();
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

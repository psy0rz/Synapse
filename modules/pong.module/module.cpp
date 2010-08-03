/** \file
A pong game proof of concept.

Used in combination with pong.html or any other frontend someone might come up with ;)


*/
#include "synapse.h"
#include <time.h>


using namespace std;
using namespace boost;


bool shutdown;

SYNAPSE_REGISTER(module_Init)
{
	Cmsg out;

	shutdown=false;

	// one game loop thread, one incoming event handle thread
	out.clear();
	out.event="core_ChangeModule";
	out["maxThreads"]=2;
	out.send();

	out.clear();
	out.event="core_ChangeSession";
	out["maxThreads"]=2;
	out.send();

	//tell the rest of the world we are ready for duty
	//(the core will send a timer_Ready)
	out.clear();
	out.event="core_Ready";
	out.send();
}

class Cplayer
{
	public:

	int id; //session id
	string name;
	int x;
	int y;

	Cplayer(int id, string name)
	{
		this->id=id;
		this->name=name;
		x=0;
		y=0;
	}
};

class Cpong
{
	typedef map<int,Cplayer> CplayerMap;
	CplayerMap playerMap;

	int id;
	string name;

	enum eStatus
	{
		NEW,
		RUNNING,
		ENDED
	};
	eStatus status;

	public:

	Cpong()
	{
		status=NEW;
	}

	void init(int id, string name)
	{
		this->id=id;
		this->name=name;
	}

	void sendStatus()
	{
		Cmsg out;
		out.event="pong_Status";
		out["id"]=id;
		out["name"]=name;
		out["status"]=status;
		out.send();
	}

	void addPlayer(Cplayer player)
	{
		if (playerMap.find(player.id)== playerMap.end())
		{
			playerMap[player.id]=player;
		}
		sendStatus();
	}


};

mutex threadMutex;
typedef map<int,Cpong> CpongMap ;
CpongMap pongMap;


/** Creates a new game on behave or \c src
 *
 */
SYNAPSE_REGISTER(pong_NewGame)
{
	lock_guard<mutex> lock(threadMutex);
	if (pongMap.find(msg.src)== pongMap.end())
	{
		pongMap[msg.src].init(msg.src, "game of "+msg["name"].str());
		pongMap[msg.src].addPlayer(Cplayer(msg.src, msg["name"]));
	}
	else
	{
		msg.returnError("You already created a game!");
	}
}


SYNAPSE_REGISTER(module_Shutdown)
{
	shutdown=true;
}



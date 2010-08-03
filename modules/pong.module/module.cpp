/** \file
A pong game proof of concept.

Used in combination with pong.html or any other frontend someone might come up with ;)


*/
#include "synapse.h"
#include <time.h>

namespace pong
{

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

		//client send-only events:
		out.clear();
		out.event="core_ChangeEvent";
		out["modifyGroup"]=	"modules";
		out["sendGroup"]=	"anonymous";
		out["recvGroup"]=	"modules";
		out["event"]=	"pong_NewGame";			out.send();
		out["event"]=	"pong_GetGames";		out.send();
		out["event"]=	"pong_JoinGame";		out.send();
		out["event"]=	"pong_StartGame";		out.send();

		//client receive-only events:
		out.clear();
		out.event="core_ChangeEvent";
		out["modifyGroup"]=	"modules";
		out["sendGroup"]=	"modules";
		out["recvGroup"]=	"anonymous";
		out["event"]=	"pong_GameStatus";		out.send();
		out["event"]=	"pong_GameDeleted";		out.send();


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

		Cplayer()
		{
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

		//ball (make objects for this too, later?)
		int ballX;
		int ballY;

		//field size
		int width;
		int height;

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

		//broadcasts a gamestatus, so that everyone knows the game exists.
		void sendStatus(int dst=0)
		{
			Cmsg out;
			out.event="pong_GameStatus";
			out["id"]=id;
			out["name"]=name;
			out["status"]=status;
			for (CplayerMap::iterator I=playerMap.begin(); I!=playerMap.end(); I++)
			{
				out["players"].list().push_back(I->second.name);
			}
			out.dst=dst;
			out.send();
		}

		void addPlayer(int id, string name)
		{
			if (playerMap.find(id)== playerMap.end())
			{
				playerMap[id].name=name;
				playerMap[id].id=id;
				sendStatus();
			}
			else
			{
				throw(runtime_error("You're already in this game?"));
			}
		}

		void delPlayer(int id)
		{
			if (playerMap.find(id)!= playerMap.end())
			{
				playerMap.erase(id);
				sendStatus();
			}
		}



		void start()
		{
			status=RUNNING;
			sendStatus();
		}

		~Cpong()
		{
			Cmsg out;
			out.event="pong_GameDeleted";
			out["id"]=id;
			out.send();
		}
	};

	mutex threadMutex;
	typedef map<int,Cpong> CpongMap ;
	CpongMap pongMap;


	// Get a reference to a pong game or throw exception
	Cpong & getPong(int id)
	{
		if (pongMap.find(id)== pongMap.end())
			throw(runtime_error("Game not found!"));

		return (pongMap[id]);
	}

	/** Creates a new game on behave of \c src
	 *
	 */
	SYNAPSE_REGISTER(pong_NewGame)
	{
		lock_guard<mutex> lock(threadMutex);
		if (msg["name"].str()=="")
		{
			throw(runtime_error("Please supply your name when creating a game."));
		}

		if (pongMap.find(msg.src)== pongMap.end())
		{
			pongMap[msg.src].init(msg.src, msg["name"].str());
			pongMap[msg.src].addPlayer(msg.src, msg["name"]);
		}
		else
		{
			throw(runtime_error("You already own a game!"));
		}
	}

	/** Lets \c src join a game
	 *
	 */
	SYNAPSE_REGISTER(pong_JoinGame)
	{
		lock_guard<mutex> lock(threadMutex);
		getPong(msg["id"]).addPlayer(msg.src, msg["name"]);
	}

	/** Starts game of \c src
	 *
	 */
	SYNAPSE_REGISTER(pong_StartGame)
	{
		lock_guard<mutex> lock(threadMutex);
		getPong(msg.src).start();
	}

	/** Returns status of all games to client.
	 *
	 */
	SYNAPSE_REGISTER(pong_GetGames)
	{
		lock_guard<mutex> lock(threadMutex);
		for (CpongMap::iterator I=pongMap.begin(); I!=pongMap.end(); I++)
		{
			I->second.sendStatus(msg.src);
		}
	}

	SYNAPSE_REGISTER(module_SessionEnd)
	{
		lock_guard<mutex> lock(threadMutex);

		//leave all games
		for (CpongMap::iterator I=pongMap.begin(); I!=pongMap.end(); I++)
		{
			I->second.delPlayer(msg.src);
		}

		//if the person owns a game, deleted it.
		if (pongMap.find(msg.src)!= pongMap.end())
		{
			pongMap.erase(msg.src);
		}
	}


	SYNAPSE_REGISTER(module_Shutdown)
	{
		shutdown=true;
	}


}

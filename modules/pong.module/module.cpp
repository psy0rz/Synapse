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
		out["event"]=	"pong_DelGame";			out.send();
		out["event"]=	"pong_GetGames";		out.send();
		out["event"]=	"pong_JoinGame";		out.send();
		out["event"]=	"pong_StartGame";		out.send();
		out["event"]=	"pong_SetPosition";		out.send();

		//client receive-only events:
		out.clear();
		out.event="core_ChangeEvent";
		out["modifyGroup"]=	"modules";
		out["sendGroup"]=	"modules";
		out["recvGroup"]=	"anonymous";
		out["event"]=	"pong_GameStatus";		out.send();
		out["event"]=	"pong_GameDeleted";		out.send();
		out["event"]=	"pong_GameJoined";		out.send();
		out["event"]=	"pong_RunStep";			out.send();

		//start game engine
		out.clear();
		out.event="pong_Engine";
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
		bool changed;

		Cplayer()
		{
			x=0;
			y=0;
			changed=true;
		}

		void setPosition(int x, int y)
		{
			if (this->x != x || this->y!=y)
			{
				this->x=x;
				this->y=y;
				changed=true;
			}
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

		//map settings
		int width;
		int height;
		int stepSize;


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
			width=10000;
			height=10000;
			stepSize=100;
			ballX=0;
			ballY=0;
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

		void addPlayer(int playerId, string name)
		{
			if (name=="")
				throw(runtime_error("Please enter your name before joining."));

			if (playerMap.find(playerId)== playerMap.end())
			{
				playerMap[playerId].name=name;
				playerMap[playerId].id=playerId;
				sendStatus();
				Cmsg out;
				out.event="pong_GameJoined";
				out["id"]=id;
				out.dst=playerId;
				out.send();
			}
			else
			{
				throw(runtime_error("You're already in this game?"));
			}
		}

		// Get a reference to a player or throw exception
		Cplayer & getPlayer(int id)
		{
			if (playerMap.find(id)== playerMap.end())
				throw(runtime_error("You're not in this game!"));

			return (playerMap[id]);
		}


		void setPlayerPosition(int playerId, int x, int y)
		{

				getPlayer(playerId).setPosition(x,y);
//			}
//			catch(...)
//			{
//
//			}
		}


		void delPlayer(int playerId)
		{
			if (playerMap.find(playerId)!= playerMap.end())
			{
				playerMap.erase(playerId);
				sendStatus();
			}

			//last player deleted?
			if (playerMap.empty())
			{
				//self destruct this game
				Cmsg out;
				out.event="pong_DelGame";
				out.src=id;
				out.send();
			}
		}


		void start()
		{
			status=RUNNING;
			sendStatus();
		}


		//runs the simulation one step, call this periodically
		void runStep()
		{
			//do calculations
			ballX=(ballX+stepSize)%width;
			ballY=(ballY+stepSize)%height;

			//send out data
			Cmsg out;
			out.event="pong_RunStep";

			out.list().push_back(ballX);
			out.list().push_back(ballY);

			//collect all positions of all players:
			for (CplayerMap::iterator I=playerMap.begin(); I!=playerMap.end(); I++)
			{
				//for efficiency sake, just send a array with a specified format:
				if (I->second.changed)
				{
					out.list().push_back(I->second.id);
					out.list().push_back(I->second.x);
					out.list().push_back(I->second.y);
					I->second.changed=false;
				}
			}


			//send the event to all the players:
			for (CplayerMap::iterator I=playerMap.begin(); I!=playerMap.end(); I++)
			{
				out.dst=I->second.id;
				out.send();
			}
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

	SYNAPSE_REGISTER(pong_Engine)
	{
		bool idle;
		while(!shutdown)
		{
			idle=true;
			{
				lock_guard<mutex> lock(threadMutex);
				for (CpongMap::iterator I=pongMap.begin(); I!=pongMap.end(); I++)
				{
					I->second.runStep();
					idle=false;
				}
			}

			if (!idle)
				usleep(50000);
			else
				sleep(1); //nothing to do, dont eat all the cpu ..
		}
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

	SYNAPSE_REGISTER(pong_DelGame)
	{
		lock_guard<mutex> lock(threadMutex);
		pongMap.erase(msg.src);
	}

	/** Lets \c src join a game
	 *
	 */
	SYNAPSE_REGISTER(pong_JoinGame)
	{
		lock_guard<mutex> lock(threadMutex);
		//leave all other games
		for (CpongMap::iterator I=pongMap.begin(); I!=pongMap.end(); I++)
		{
			I->second.delPlayer(msg.src);
		}
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

	/** Sets position of \c src in the specified game id.
	 *
	 */
	SYNAPSE_REGISTER(pong_SetPosition)
	{
		lock_guard<mutex> lock(threadMutex);
		getPong(msg["id"]).setPlayerPosition(msg.src, msg["x"], msg["y"]);
	}


	SYNAPSE_REGISTER(module_SessionEnded)
	{
		lock_guard<mutex> lock(threadMutex);

		//leave all games
		for (CpongMap::iterator I=pongMap.begin(); I!=pongMap.end(); I++)
		{
			I->second.delPlayer(msg["session"]);
		}
	}


	SYNAPSE_REGISTER(module_Shutdown)
	{
		shutdown=true;
	}


}

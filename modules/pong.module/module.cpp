/** \file
A pong game proof of concept.

Used in combination with pong.html or any other frontend someone might come up with ;)


*/
#include "synapse.h"
#include <time.h>
#include <set>
#include <boost/date_time/posix_time/posix_time.hpp>


namespace pong
{

	using namespace std;
	using namespace boost;
	using namespace boost::posix_time;

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
		out["event"]=	"pong_Positions";		out.send();

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


	//position of an object with motion prediction info
	class Cposition
	{
		private:
		int x;
		int y;
		int xSpeed; //pixels per second
		int ySpeed;
		ptime startTime;
		bool changed;

		public:

		bool isChanged()
		{
			return (changed);
		}

		Cposition()
		{
			set(0,0,0,0);
		}

		void set(int x, int y, int xSpeed, int ySpeed)
		{
			this->x=x;
			this->y=y;
			this->xSpeed=xSpeed;
			this->ySpeed=ySpeed;
			changed=true;
			startTime=microsec_clock::local_time();
		}

		//get the position, according to calculations of speed and time.
		void get(int & currentX, int & currentY, int & currentXspeed, int & currentYspeed)
		{
			time_duration td=(microsec_clock::local_time()-startTime);
			currentX=x+((xSpeed*td.total_nanoseconds())/1000000000);
			currentY=y+((ySpeed*td.total_nanoseconds())/1000000000);
			currentXspeed=xSpeed;
			currentYspeed=ySpeed;
			changed=false;
		}
	};

	class Cplayer
	{
		public:

		string name;
		Cposition position;

		Cplayer()
		{
		}

	};

	typedef map<int,Cplayer> CplayerMap;
	CplayerMap playerMap;

	class Cpong
	{

		int id;
		string name;

		Cposition ballPosition;

		//map settings
		int width;
		int height;

		typedef set<int> CplayerIds;
		CplayerIds playerIds;

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
			ballPosition.set(0,0,1000,2000);
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
			for (CplayerIds::iterator I=playerIds.begin(); I!=playerIds.end(); I++)
			{
				out["players"].list().push_back(playerMap[*I].name);
			}
			out.dst=dst;
			out.send();
		}

		void addPlayer(int playerId, string name)
		{
			if (playerIds.find(playerId)== playerIds.end())
			{
				if (name=="")
				{
					throw(runtime_error("Please enter a name before joining the game."));
				}

				playerMap[playerId].name=name;
				playerIds.insert(playerId);
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

		void delPlayer(int playerId)
		{
			if (playerIds.find(playerId)!= playerIds.end())
			{
				playerIds.erase(playerId);
				sendStatus();
			}

			//last player deleted?
			if (playerIds.empty())
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
			{
				//do calculations:
				int ballX,ballY,ballXspeed, ballYspeed;
				bool bounced=false;
				ballPosition.get(ballX,ballY, ballXspeed, ballYspeed);

				//bounce the ball of the walls?
				if (ballX>width)
				{
					ballX=width-(ballX-width);
					ballXspeed=ballXspeed*-1;
					bounced=true;
				}
				else if (ballX<0)
				{
					ballX=ballX*-1;
					ballXspeed=ballXspeed*-1;
					bounced=true;
				}
				if (ballY>height)
				{
					ballY=height-(ballY-height);
					ballYspeed=ballYspeed*-1;
					bounced=true;
				}
				else if (ballY<0)
				{
					ballY=ballY*-1;
					ballYspeed=ballYspeed*-1;
					bounced=true;
				}

				//it bounced?
				if (bounced)
				{
					//update position
					ballPosition.set(ballX,ballY, ballXspeed, ballYspeed);
				}
			}

			//send a update to all players, each player gets a uniq list, sending only the minimum amount of data
			//TODO: possible optimization if there are lots of players, for now the code is clean rather that uber efficient
			map<int,Cmsg> outs; //list of output messages, one for each player
			int x,y,xSpeed,ySpeed;

			//send everyone a ball position change
			if (ballPosition.isChanged())
			{
				ballPosition.get(x,y,xSpeed,ySpeed);
				for (CplayerIds::iterator I=playerIds.begin(); I!=playerIds.end(); I++)
				{
					outs[*I].list().push_back(0);
					outs[*I].list().push_back(x);
					outs[*I].list().push_back(y);
					outs[*I].list().push_back(xSpeed);
					outs[*I].list().push_back(ySpeed);
				}
			}

			//check which players have an updated position
			for (CplayerIds::iterator I=playerIds.begin(); I!=playerIds.end(); I++)
			{
				if (playerMap[*I].position.isChanged())
				{
					playerMap[*I].position.get(x,y,xSpeed,ySpeed);

					//add the update to the out-message for all players, but never tell them their own info.
					for (CplayerIds::iterator sendI=playerIds.begin(); sendI!=playerIds.end(); sendI++)
					{
						if (*sendI!=*I)
						{
							outs[*sendI].list().push_back(*I);
							outs[*sendI].list().push_back(x);
							outs[*sendI].list().push_back(y);
							outs[*sendI].list().push_back(xSpeed);
							outs[*sendI].list().push_back(ySpeed);
						}
					}
				}
			}

			//now do the actual sending
			for (CplayerIds::iterator I=playerIds.begin(); I!=playerIds.end(); I++)
			{
				//only if there actually is something to send
				if (!outs[*I].list().empty())
				{
					outs[*I].event="pong_Positions";
					outs[*I].dst=*I;
					outs[*I].send();
				}
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
			pongMap[msg.src].addPlayer(msg.src,msg["name"].str());
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

	/** Sets position of \c src
	 *
	 */
	SYNAPSE_REGISTER(pong_SetPosition)
	{
		lock_guard<mutex> lock(threadMutex);
		if (msg.list().size()==4)
		{
			Cmsg::iteratorList I;
			I=msg.list().begin();
			int x,y,xSpeed,ySpeed;
			x=*I;
			I++;
			y=*I;
			I++;
			xSpeed=*I;
			I++;
			ySpeed=*I;

			playerMap[msg.src].position.set(x, y, xSpeed, ySpeed);
		}
	}


	SYNAPSE_REGISTER(module_SessionEnded)
	{
		lock_guard<mutex> lock(threadMutex);

		//leave all games
		for (CpongMap::iterator I=pongMap.begin(); I!=pongMap.end(); I++)
		{
			I->second.delPlayer(msg["session"]);
		}
		//remove from map
		playerMap.erase(msg["session"]);
	}


	SYNAPSE_REGISTER(module_Shutdown)
	{
		shutdown=true;
	}


}

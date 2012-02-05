#ifndef CPAPEROBJECT
#define CPAPEROBJECT

#include "cpaperclient.h"
#include "csharedobject.h"
#include "cconfig.h"

#include <string>

namespace paper
{
	using namespace std;

	//a server side piece of paper
	class CpaperObject : public synapse::CsharedObject<CpaperClient>
	{
		private:
		bool mExporting;
		synapse::Cconfig mDrawing;

		//called when the drawing-data of the drawing is modified. (e.g. new export is needed)
		void changed();


		public:
		CpaperObject();

		//send message to all clients
		void sendAll(Cmsg & msg);

		//get filenames, relative to wwwdir, or relative to synapse main dir.
		//these probably are going to give different results when papers are made private.
		string getSvgFilename(bool www=false);
		string getPngFilename(bool www=false);
		string getThumbFilename(bool www=false);
		string getHtmlFilename(bool www=false);

		void createHtml();

		//the paper is created for the first time.
		void create();

		//called by the object manager to get interesting metadata about this object
		void getInfo(Cvar & var);


		//called when exporting is done
		void exported();

		//export the drawing to svg and png files.
		//sends various a paper_Exported event when done.
		void saveExport(bool send=true);

		void execEnded(Cvar & var);

		void execError(Cvar & var);

		void save(string path);

		void load(string path);

		//authenticates clientId with key
		void login(int clientId, string key);

		//changes authentication keys and authorization
		//rights is just a hasharray
		//Specifying clientId checks if the client is allowed to give these rights
		//specifying an empty rights-array will delete the key
		void changeAuth(int clientId, string key, Cvar & rights);

		//get all authorisation and authentication info
		void getAuth(int clientId);

		//send the drawing commands to all clients, except clientId
		//(use clientId 0 if your want the message send to all clients)
		void serverDraw(Cmsg out, int clientId=0 );;

		//get an iterator to specified element id.
		//throws error if not found.
		Cvar::iterator getElement(const string & id);

		//transform the specified elementid into a message-data that can be send to clients
		//sets:
		//["element"]=elementtype
		//["set"][attributename]=attributevalue
		//["id"]=id
		//["beforeId"]=id of element this element comes before
		void element2msg(const string & id, Cmsg & msg);

		//resend all data to dst
		void reload(int dst);

		//process drawing data received from a client store it, and relay it to other clients via serverDraw
		//if a client is not authorized to do certain stuff, an exception is thrown
		void clientDraw(Cmsg & msg);

		//virtual void addClient(int id);

		virtual bool isIdle();

	};
};

#endif


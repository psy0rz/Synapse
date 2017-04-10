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


/** \file
Asterisk Control Module.

Uses the AMI module (and maybe others) to track and control asterisk.

The complex flow of asterisk events is converted to a few simple events, usable in operator panels and other apps.

\code
To capture test-data from a asterisk server:
	script -c telnet 1.2.3.4 5038 -t ami.txt 2> ami.timing

and login with:
 Action: Login
 ActionID: Login
 Events: on
 Secret: password
 UserName: username



To setup a fake server replaying this:

 tcpserver 0.0.0.0 5555 scriptreplay ami.timing ami.txt


\endcode



*/



#include "synapse.h"
#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include "cserver.h"
#include "cdevice.h"
#include "cchannel.h"
#include "csession.h"

#define ASTERISK_AUTH "9999"

namespace asterisk
{
	using namespace std;
	using namespace boost;

	CserverMan serverMan("var/asterisk/state_db.conf");


	SYNAPSE_REGISTER(module_Init)
	{
		Cmsg out;





		out.clear();
		out.event="core_ChangeModule";
		out["maxThreads"]=1; //NOTE: this module is single threaded only!
		out.send();

		out.clear();
		out.event="core_ChangeSession";
		out["maxThreads"]=1;
		out.send();

		out.clear();
		out.event="core_LoadModule";
		out["path"]="modules/ami.module/libami.so";
		out.send();


		out.clear();
		out.event="core_LoadModule";
		out["path"]="modules/timer.module/libtimer.so";
		out.send();


		out.clear();
		out.event="core_LoadModule";
		out["path"]="modules/http_json.module/libhttp_json.so";
		out.send();


		out.clear();


		//send by anonymous only:
		out.event="core_ChangeEvent";
		out["modifyGroup"]=	"modules";
		out["sendGroup"]=	"anonymous";
		out["recvGroup"]=	"modules";

		out["event"]=		"asterisk_authReq";
		out.send();

		out["event"]=		"asterisk_refresh";
		out.send();

		out["event"]=		"asterisk_Call";
		out.send();

		out["event"]=		"asterisk_Hangup";
		out.send();

		out["event"]=		"asterisk_Bridge";
		out.send();

		out["event"]=		"asterisk_Park";
		out.send();

		out["event"]=		"asterisk_Test";
		out.send();

		//receive by anonymous only:
		out.event="core_ChangeEvent";
		out["modifyGroup"]=	"modules";
		out["sendGroup"]=	"modules";
		out["recvGroup"]=	"anonymous";


		out["event"]=		"asterisk_debugChannel";
		out.send();


		out["event"]=		"asterisk_authCall";
		out.send();

		out["event"]=		"asterisk_authOk";
		out.send();

		out["event"]=		"asterisk_updateChannel";
		out.send();

		out["event"]=		"asterisk_delChannel";
		out.send();

		out["event"]=		"asterisk_updateDevice";
		out.send();

		out["event"]=		"asterisk_delDevice";
		out.send();

		out["event"]=		"asterisk_reset";
		out.send();

		out["event"]=		"asterisk_resetComplete";
		out.send();

		out["event"]=		"asterisk_State";
		out.send();

		out["event"]=		"asterisk_speedDialList";
		out.send();

		out["event"]=		"asterisk_phoneBookList";
		out.send();
	}


	string getDeviceIdFromChannel(string channel)
	{
		//a channel can be either:
		// technology/number-random
		//or
		// agent/number
		smatch what;
		if (!regex_search(
			channel,
			what,
			boost::regex("^(.*)-([^-]*)$")
		))
		{
			//return whole string, appearently there is no random channel identifier added. (happens with agents)
			return (channel);
//			WARNING("Invalid channel: " << channel << ", using 'unknown' instead");
//	/		return (string("unknown"));
		}
		else
		{
			return (what[1]);
		}
	}






	SYNAPSE_REGISTER(ami_Ready)
	{
		///if ami is ready, we are ready ;)
		Cmsg out;
		out.clear();
		out.event="core_Ready";
		out.send();

		//load config, which in turn connects servers
		out.clear();
		out.event="asterisk_Config";
		out.send();
	}




	SYNAPSE_REGISTER(module_SessionStart)
	{
		//new session that is started with server info
		if (msg.isSet("server"))
		{
			//setup a new server object
			CserverPtr serverPtr=serverMan.getServerPtr(msg.dst);

			serverPtr->id=msg["server"]["id"].str();
			serverPtr->username=msg["server"]["username"].str();
			serverPtr->password=msg["server"]["password"].str();
			serverPtr->host=msg["server"]["host"].str();
			serverPtr->port=msg["server"]["port"].str();
			serverPtr->group_default=msg["server"]["group_default"].str();
			serverPtr->group_regex=msg["server"]["group_regex"].str();
			serverPtr->device_show_regex=msg["server"]["device_show_regex"].str();

			serverPtr->last_net_error="Connecting";
			serverPtr->setState(Cserver::CONNECTING);

			//instruct ami to connect to the server
			Cmsg out;
			out.clear();
			out.event="ami_Connect";
			out.src=msg.dst;
			out["host"]=msg["server"]["host"].str();
			out["port"]=msg["server"]["port"].str();
			out.send(0,Cmsg::INFO);
		}
	}

	//a session owned by us ended, remove the corresponding server.
	SYNAPSE_REGISTER(module_SessionEnd)
	{
		//since the ami module also sees a module_SessionEnded, it will automaticly disconnect the server
		serverMan.delServer(msg.dst);
	}

	//a session owned by another module ended. check if we created local stuff that we need to delete.
	SYNAPSE_REGISTER(module_SessionEnded)
	{
		serverMan.delSession(msg["session"]);
	}


	SYNAPSE_REGISTER(ami_Connected)
	{
		//ami is connected, now login with the right credentials for this server:
		Cmsg out;
		out.clear();
		out.src=msg.dst;
		out.event="ami_Action";
		out["Action"]="Login";
		out["UserName"]=serverMan.getServerPtr(msg.dst)->username;
		out["Secret"]=serverMan.getServerPtr(msg.dst)->password;
		out["ActionID"]="Login";
		out["Events"]="on";
		out.send(0,Cmsg::INFO);

		serverMan.getServerPtr(msg.dst)->setState(Cserver::AUTHENTICATING);

	}

	SYNAPSE_REGISTER(ami_Response_Success)
	{

		//AMI is broken by design: why didnt they just use a Event, instead of defining the format of a Response exception. Now we need to use the ActionID and do extra marshhalling:
		//SIPshowPeer response
		if (msg["ActionID"].str()=="SIPshowPeer")
		{
			string deviceId=msg["Channeltype"].str()+"/"+msg["ObjectName"].str();
			CdevicePtr devicePtr=serverMan.getServerPtr(msg.dst)->getDevicePtr(deviceId);
			devicePtr->setExten(msg["ObjectName"]);

			//NOTE: we handle Unmonitored sip peers as online, while we dont actually know if its online or not.
			if (msg["Status"].str().find("OK")==0 || msg["Status"].str().find("Unmonitored")==0 )//||  msg["Status"].str().find("UNKNOWN")==0)
				devicePtr->setOnline(true);
			else
				devicePtr->setOnline(false);

			if (msg["Callerid"].str() != "\"\" <>")
			{
				//split up the the callerid+callerIdname string
				//Why the heck are the asterisk folks pre-formatting it like this?? Everywhere else in the ami a caller id is just a number instead of a preformatted string!
				smatch what;
				if (regex_search(
					msg["Callerid"].str(),
					what,
					boost::regex("\"(.*)\" <(.*)>")
				))
				{
					devicePtr->setCallerIdName(what[1]);
					devicePtr->setCallerId(what[2]);
				}
				else
				{
					//cant parse somehow? just set the whole string as callerIdName
					devicePtr->setCallerIdName(msg["CallerId"]);
				}
			}
			else
				devicePtr->setCallerIdName(msg["ObjectName"]);

			if (msg["ToHost"].str() != "")
				devicePtr->setTrunk(true);
			else
				devicePtr->setTrunk(false);


		}
		//login response
		else if (msg["ActionID"].str()=="Login")
		{
			serverMan.getServerPtr(msg.dst)->setState(Cserver::AUTHENTICATED);
			Cmsg out;

			//learn all SIP peers as soon as we login
			out.clear();
			out.src=msg.dst;
			out.event="ami_Action";
			out["Action"]="SIPPeers";
			out.send(0,Cmsg::INFO);

			//learn all IAX peers as soon as we login
			out.clear();
			out.src=msg.dst;
			out.event="ami_Action";
			out["Action"]="IAXPeers";
			out.send(0,Cmsg::INFO);

			//learn all ZAP channels as soon as we login
			//NOTE: gives useless info
			// out.clear();
			// out.src=msg.dst;
			// out.event="ami_Action";
			// out["Action"]="ZapShowChannels";
			// out.send();

			//learn all DAHDI channels as soon as we login
			//NOTE: gives useless info
			// out.clear();
			// out.src=msg.dst;
			// out.event="ami_Action";
			// out["Action"]="DahdiShowChannels";
			// out.send();

			//learn current channel status as soon as we login
			out.clear();
			out.src=msg.dst;
			out.event="ami_Action";
			out["Action"]="Status";
			out.send(0,Cmsg::INFO);

		}


	}

	SYNAPSE_REGISTER(ami_Response_Error)
	{
		serverMan.getServerPtr(msg.dst)->last_ami_error=msg["Message"].str();
	}


	SYNAPSE_REGISTER(ami_Disconnected)
	{
		//since we're disconnected, clear all devices/channels
		serverMan.getServerPtr(msg.dst)->clear();
		serverMan.getServerPtr(msg.dst)->last_net_error=msg["reason"].str();
		serverMan.getServerPtr(msg.dst)->setState(Cserver::CONNECTING);

		//ami reconnects automaticly
	}





	//we got a response to our SIPPeers/IAXpeers request.
		/* IAX2 1.8:
				|ChanObjectType = peer (string)
				|Channeltype = IAX2 (string)
				|Dynamic = no (string)
				|Encryption = no (string)
				|Event = PeerEntry (string)
				|IPaddress = 81.18.245.155 (string)
				|IPport = 4569 (string)
				|ObjectName = flexvoice/201-CDS_Datux (string)
				|Status = Unmonitored (string)
				|Trunk = yes (string)

			 SIP 1.8:
				|ACL = no (string)
				|ChanObjectType = peer (string)
				|Channeltype = SIP (string)
				|Dynamic = yes (string)
				|Event = PeerEntry (string)
				|Forcerport = yes (string)
				|IPaddress = -none- (string)
				|IPport = 0 (string)
				|ObjectName = 1002 (string)
				|RealtimeDevice = no (string)
				|Status = Unmonitored (string)
				|TextSupport = no (string)
				|VideoSupport = no (string)

		*/
	SYNAPSE_REGISTER(ami_Event_PeerEntry)
	{
		serverMan.getServerPtr(msg.dst)->getDevicePtr(msg["Channeltype"].str()+"/"+msg["ObjectName"].str());

		//request additional device info for this SIP peer
		//example: "SIP/604"
		if (msg["Channeltype"].str()=="SIP")
		{
			//(peerEntrys should be ALWAYS of type SIP?)
			Cmsg out;
			out.clear();
			out.src=msg.dst;
			out.dst=msg.src;
			out.event="ami_Action";
			out["Action"]="SIPshowPeer";
			out["ActionID"]="SIPshowPeer";
			out["Peer"]=msg["ObjectName"].str();
			out.send(0,Cmsg::INFO);
		}

	}

		/*
				1.8:
				|Alarm = No Alarm (string)
				|Context = phones (string)
				|DAHDIChannel = 1 (string)
				|DND = Disabled (string)
				|Event = DAHDIShowChannels (string)
				|Signalling = FXO Kewlstart (string)
				|SignallingCode = 4128 (string)
		*/
		SYNAPSE_REGISTER(ami_Event_DAHDIShowChannels)
		{

		}

	void channelStatus(Cmsg & msg)
	{
		CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid"]);
		CdevicePtr devicePtr=serverMan.getServerPtr(msg.dst)->getDevicePtr(
			getDeviceIdFromChannel(msg["Channel"])
		);

		channelPtr->setDevicePtr(devicePtr);
		channelPtr->setChannelName(msg["Channel"]);

		//States:
		// ringing: somebody is calling the device
		// in     : incoming call in progress
		// out    : outgoing call in progress
		// ''     : other (uninteresting) states

		//device is ringing
		string state;
		if (msg.isSet("State")) // 1.4
			state=msg["State"].str();

		if (msg.isSet("ChannelStateDesc")) // 1.8
			state=msg["ChannelStateDesc"].str();



		if (state=="Ringing")
		{
			channelPtr->setState("ring_in");
		}
		//device is dialing (ringing other side)
		else if (state=="Ring")
		{
			channelPtr->setState("ring_out");
		}
		//device is connected with other side
		else if (state=="Up")
		{
			if (channelPtr->getState()=="ring_in")
			{
				channelPtr->setState("in");
			}
			else
			{
				channelPtr->setState("out");
			}
		}
		//device is down/other stuff
		else
		{
			channelPtr->setState("");
		}

		//NOTE: whats with all the different namings and <unknown> vs <Unknown> in Newcallerid?

		if (msg.isSet("CallerIDNum"))
		{
			if (msg["CallerIDNum"].str() == "<unknown>")
				;//channelPtr->setCallerId("");
			else
				channelPtr->setCallerId(msg["CallerIDNum"]);
		}

		if (msg.isSet("CallerID"))
		{
			if (msg["CallerID"].str() == "<unknown>")
				;//channelPtr->setCallerId("");
			else
				channelPtr->setCallerId(msg["CallerID"]);
		}

		if (msg.isSet("CallerIDName"))
		{
			if (msg["CallerIDName"].str() == "<unknown>")
				channelPtr->setCallerIdName("");
			else
				channelPtr->setCallerIdName(msg["CallerIDName"]);
		}

		if (msg.isSet("Exten")) //1.8
		{
			if (channelPtr->getFirstExtension()=="")
			{
				channelPtr->setFirstExtension(msg["Exten"]);
			}
		}


//		devicePtr->sendChanges();
		channelPtr->sendDebug(msg, msg.dst);


	}


	// new channel created
	SYNAPSE_REGISTER(ami_Event_Newchannel)
	{

	/*	Event: Newchannel
		Privilege: call,all
		Channel: SIP/604-00000069
		State: Down
		CallerIDNum: 604
		CallerIDName: Edwin (draadloos)
		Uniqueid: 1269871368.143

		Event: Newchannel
		Privilege: call,all
		Channel: SIP/605-0000006a
		State: Down
		CallerIDNum: <unknown>
		CallerIDName: <unknown>
		Uniqueid: 1269871368.144

				1.8 dahdi horn pickup:
				|Account =  (string)
				|CallerIDName = <unknown> (string)
				|CallerIDNum = <unknown> (string)
				|Channel = DAHDI/1-1 (string)
				|ConnectedLineName = <unknown> (string)
				|ConnectedLineNum = <unknown> (string)
				|Event = Status (string)
				|Privilege = Call (string)
				|State = Rsrvd (string)
				|Uniqueid = 1352387005.54 (string)


		1.8 meetme call. new channels always specify Exten
		Newchannel
		AccountCode:
		CallerIDName:links
		CallerIDNum:100
		Channel:SIP/100-00000035
		ChannelState:0
		ChannelStateDesc:Down
		Context:from-internal
		Exten:600
		Privilege:call,all
		State:
		Uniqueid:1395247002.54
		deviceId:SIP/100



				*/

		channelStatus(msg);
	}

	// initial channel status event, requested just after connecting to an asterisk server.
	SYNAPSE_REGISTER(ami_Event_Status)
	{
	// 	Event: Status
	// 	Privilege: Call
	// 	Channel: SIP/601-00000048
	// 	CallerID: 601
	// 	CallerIDNum: 601
	// 	CallerIDName: <unknown>
	// 	Account:
	// 	State: Ringing
	// 	Uniqueid: 1269958019.99
	//
	// 	Event: Status
	// 	Privilege: Call
	// 	Channel: SIP/605-00000047
	// 	CallerID: 605
	// 	CallerIDNum: 605
	// 	CallerIDName: <unknown>
	// 	Account:
	// 	State: Up
	// 	Link: SIP/604-00000046
	// 	Uniqueid: 1269958018.98
/*
				1.8 dahdi:
				|Account =  (string)
				|CallerIDName = <unknown> (string)
				|CallerIDNum = <unknown> (string)
				|Channel = DAHDI/1-1 (string)
				|ConnectedLineName = <unknown> (string)
				|ConnectedLineNum = <unknown> (string)
				|Event = Status (string)
				|Privilege = Call (string)
				|State = Rsrvd (string)
				|Uniqueid = 1352387096.56 (string)

		1.8 normal channel with meetme
			|Accountcode =  (string)
		 |CallerIDName = rechts (string)
		 |CallerIDNum = 101 (string)
		 |Channel = SIP/101-0000009a (string)
		 |ChannelState = 6 (string)
		 |ChannelStateDesc = Up (string)
		 |ConnectedLineName = <unknown> (string)
		 |ConnectedLineNum = <unknown> (string)
		 |Context = from-internal (string)
		 |Event = Status (string)
		 |Extension = STARTMEETME (string)
		 |Priority = 4 (string)
		 |Privilege = Call (string)
		 |Seconds = 7 (string)
		 |Uniqueid = 1395315196.165 (string)

*/
		channelStatus(msg);
	}

	// channel status is changing
	SYNAPSE_REGISTER(ami_Event_Newstate)
	{
		/*
		|CallerID = 605 (string)
		|CallerIDName = <unknown> (string)
		|Channel = SIP/605-0000005b (string)
		|Event = Newstate (string)
		|Privilege = call,all (string)
		|State = Ringing (string)
		|Uniqueid = 1269870185.119 (string)                                                                                                     */

		channelStatus(msg);
    serverMan.sendChanges();

	}


	SYNAPSE_REGISTER(ami_Event_Hangup)
	{
		CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid"]);
		channelPtr->sendDebug(msg, msg.dst);
		serverMan.getServerPtr(msg.dst)->delChannel(msg["Uniqueid"]);
	//	serverMap[msg.dst].getDevicePtr(getDeviceId(msg["Channel"]))->sendStatus();
	//	INFO("\n" << serverMap[msg.dst].getStatusStr());

	}

	SYNAPSE_REGISTER(ami_Event_PeerStatus)
	{

		CdevicePtr devicePtr=serverMan.getServerPtr(msg.dst)->getDevicePtr(msg["Peer"]);
		if (msg["PeerStatus"].str()=="Reachable" || msg["PeerStatus"].str()=="Registered" )
			devicePtr->setOnline(true);
		else
			devicePtr->setOnline(false);

//		devicePtr->sendChanges();

	}



	SYNAPSE_REGISTER(ami_Event_Link)
	{
	/*	Event: Link
		Privilege: call,all
		Channel1: SIP/604-00000022
		Channel2: SIP/605-00000023
		Uniqueid1: 1269864649.42
		Uniqueid2: 1269864649.43
		CallerID1: 604
		CallerID2: 605*/
		CchannelPtr channelPtr1=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid1"]);
		CchannelPtr channelPtr2=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid2"]);

		channelPtr1->setLinkPtr(channelPtr2);
		channelPtr2->setLinkPtr(channelPtr1);


		channelPtr1->sendDebug(msg, msg.dst);
		channelPtr2->sendDebug(msg, msg.dst);
	}



	SYNAPSE_REGISTER(ami_Event_Bridge)
	{
		// 0x103fc30 SEND ami_Event_Bridge FROM 3:module@ami TO 6:module@asterisk (map:)
		//  |Bridgestate = Link (string)
		//  |Bridgetype = core (string)
		//  |CallerID1 = 100 (string)
		//  |CallerID2 = 101 (string)
		//  |Channel1 = SIP/100-00000034 (string)
		//  |Channel2 = SIP/101-00000035 (string)
		//  |Event = Bridge (string)
		//  |Privilege = call,all (string)
		//  |Uniqueid1 = 1396706816.59 (string)
		//  |Uniqueid2 = 1396706816.60 (string)
		CchannelPtr channelPtr1=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid1"]);
		CchannelPtr channelPtr2=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid2"]);

		channelPtr1->setLinkPtr(channelPtr2);
		channelPtr2->setLinkPtr(channelPtr1);


		channelPtr1->sendDebug(msg, msg.dst);
		channelPtr2->sendDebug(msg, msg.dst);
	}



	SYNAPSE_REGISTER(ami_Event_Unlink)
	{
		/*Event: Unlink
		Privilege: call,all
		Channel1: SIP/604-00000020
		Channel2: SIP/605-00000021
		Uniqueid1: 1269864479.40
		Uniqueid2: 1269864479.41
		CallerID1: 604
		CallerID2: 605*/
		CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid1"]);
		channelPtr->delLink();
		channelPtr->sendDebug(msg, msg.dst);

		channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid2"]);
		channelPtr->delLink();
		channelPtr->sendDebug(msg, msg.dst);



	}



	SYNAPSE_REGISTER(ami_Event_Newexten)
	{
		// Event: Newexten
		// Privilege: call,all
		// Channel: SIP/604-00000007
		// Context: DLPN_DatuX
		// Extension: 9991234
		// Priority: 1
		// Application: Macro
		// AppData: trunkdial-failover-0.3|SIP/0858784323/9991234|mISDN/g:trunk_m1/9991234|0858784323|trunk_m1
		// Uniqueid: 1269802539.12

		/* |AppData = SIP/605 (string)
		|Application = Dial (string)
		|Channel = SIP/604-0000002f (string)
		|Context = DLPN_DatuX (string)
		|Event = Newexten (string)
		|Extension = 605 (string)
		|Priority = 1 (string)
		|Privilege = call,all (string)
		|Uniqueid = 1269866053.55 (string)*/

		//somebody dialed the special authentication number?
		if (msg["Extension"].str().find(ASTERISK_AUTH)==0)
		{
			//determine specified session number
			int sessionId=lexical_cast<int>(msg["Extension"].str().substr(strlen(ASTERISK_AUTH)));
			CserverPtr serverPtr=serverMan.getServerPtr(msg.dst);
			CchannelPtr channelPtr=serverPtr->getChannelPtr(msg["Uniqueid"]);
			CdevicePtr devicePtr=serverPtr->getDevicePtr(getDeviceIdFromChannel(msg["Channel"]));

			//do we know the specified session?
			if (serverMan.sessionExists(sessionId))
			{
				CsessionPtr sessionPtr=serverMan.getSessionPtr(sessionId);
				//session is not yet authorized?
				if (!sessionPtr->isAuthorized())
				{
					//autenticate the session
					sessionPtr->authorize(serverPtr, devicePtr);

					Cmsg out;
					out.event="asterisk_authOk";
					out.dst=sessionId;
					out["deviceId"]=devicePtr->getId();
					out["serverId"]=serverPtr->id;
					out["authCookie"]=serverMan.getAuthCookie(serverPtr->id, devicePtr->getId());
					out.send(0,Cmsg::INFO);

					//give OK feedback to caller
					serverPtr->amiRedirect(channelPtr, "from-synapse", "login_ok");
					return;
				}
			}

			//give negative feedback to caller
			//serverPtr->amiRedirect(channelPtr, "from-synapse", "login_failed");
			//sometimes is this is called multiple times, disable for now

		}

		CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid"]);
		if (channelPtr->getFirstExtension()=="")
		{
			channelPtr->setFirstExtension(msg["Extension"]);
		}

		//TOO much info..channelPtr->sendDebug(msg, msg.dst);
	}



	SYNAPSE_REGISTER(ami_Event_Dial)
	{
	//1.4:
	// 	Event: Dial
	// 	Privilege: call,all
	// 	Source: SIP/604-0000002f
	// 	Destination: SIP/605-00000030
	// 	CallerID: 604
	// 	CallerIDName: Edwin (draadloos)
	// 	SrcUniqueID: 1269866053.55
	// 	DestUniqueID: 1269866053.56

	// 	Event: Dial
	// 	Privilege: call,all
	// 	Source: SIP/604-00000031
	// 	Destination: mISDN/0-u5
	// 	CallerID: 0858784323
	// 	CallerIDName: <unknown>
	// 	SrcUniqueID: 1269866267.57
	// 	DestUniqueID: 1269866267.58

	//1.8:
	 // |CallerIDName = links (string)
	 // |CallerIDNum = 100 (string)
	 // |Channel = SIP/100-0000000e (string)
	 // |ConnectedLineName = rechts (string)
	 // |ConnectedLineNum = 101 (string)
	 // |DestUniqueID = 1395235453.15 (string)
	 // |Destination = SIP/101-0000000f (string)
	 // |Dialstring = 101 (string)
	 // |Event = Dial (string)
	 // |Privilege = call,all (string)
	 // |SubEvent = Begin (string)
	 // |UniqueID = 1395235453.14 (string)
	 //
	 // cancelling a dial in progress:
	 // |Channel = SIP/101-0000001e (string)
	 // |DialStatus = CANCEL (string)
	 // |Event = Dial (string)
	 // |Privilege = call,all (string)
	 // |SubEvent = End (string)
	 // |UniqueID = 1395236996.30 (string)

		//NOTE: a "link" for us, is something different then a link for asterisk.
		CchannelPtr channelPtr1;
		if (msg.isSet("SrcUniqueID")) //1.4
			channelPtr1=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["SrcUniqueID"]);
		else //1.8
			channelPtr1=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["UniqueID"]);

		CchannelPtr channelPtr2=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["DestUniqueID"]);
		channelPtr1->setLinkPtr(channelPtr2);
		channelPtr2->setLinkPtr(channelPtr1);


		//in case of followme and other situation its important we use the Dial callerId as well, on the originating channel (SrcUniqueID/UniqueID)
		if (msg.isSet("CallerID")) //1.4
		{
			if (msg["CallerID"].str() == "<unknown>")
				;//channelPtr->setCallerId("");
			else
				channelPtr1->setCallerId(msg["CallerID"]);
		}

		if (msg.isSet("CallerIDNum")) //1.8
		{
			channelPtr1->setCallerId(msg["CallerIDNum"]);
		}

		if (msg["CallerIDName"].str() == "<unknown>")
			channelPtr1->setCallerIdName("");
		else
			channelPtr1->setCallerIdName(msg["CallerIDName"]);


		//in 1.8 we can use ConnectedLineName and Num to set the callerid of the other channel:
		if (msg.isSet("ConnectedLineName")) //1.8
			channelPtr2->setCallerIdName(msg["ConnectedLineName"]);

		if (msg.isSet("ConnectedLineNum")) //1.8
			channelPtr2->setCallerId(msg["ConnectedLineNum"]);


		channelPtr1->sendDebug(msg, msg.dst);
		channelPtr2->sendDebug(msg, msg.dst);

	/*
		channelPtr=serverMap[msg.dst].getChannelPtr(msg["DestUniqueID"]);
		channelPtr->setLink(msg["SrcUniqueID"]);*/

	// 	CchannelPtr channelPtr=serverMap[msg.dst].getChannelPtr(msg["Destination"]);
	// 	channelPtr->setCallerId("gettingcalled\"" + msg["CallerIDName"].str() + "\" <" + msg["CallerID"].str() + ">" );

	// 	channelPtr=serverMap[msg.dst].getChannelPtr(msg["Source"]);
	// 	channelPtr->setCallerId("calling\"" + msg["CallerIDName"].str() + "\" <" + msg["CallerID"].str() + ">" );

	}

	// renaming usually happens on things like call-transfers

	SYNAPSE_REGISTER(ami_Event_Rename)
	{
		//1.4:
		// Event: Rename
		// Privilege: call,all
		// Oldname: SIP/604-00000044
		// Newname: SIP/605-00000043
		// Uniqueid: 1269956933.94

		//1.8:
		// Event: Rename
		// Channel:SIP/100-00000057
		// Newname:SIP/101-00000059
		// Privilege:call,all
		// Uniqueid:1395248644.92
		// deviceId:SIP/101

		CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid"]);
		CdevicePtr devicePtr=serverMan.getServerPtr(msg.dst)->getDevicePtr(
			getDeviceIdFromChannel(msg["Newname"])
		);

		channelPtr->setChannelName(msg["Newname"]);

		//we assume a rename only is possible for channels that are already up?
		if (channelPtr->getState()=="ringing")
		{
			channelPtr->setState("in");
		}
		else
		{
			channelPtr->setState("out");
		}
		channelPtr->setDevicePtr(devicePtr);

		if (channelPtr->getLinkPtr()==CchannelPtr())
		{
			//when we get renamed while NOT linked, we store the current callerids in the linkedcallerids.
			//after the rename we usually receive our new caller id immeadiatly
			channelPtr->setLinkCallerId(channelPtr->getCallerId());
			channelPtr->setLinkCallerIdName(channelPtr->getCallerIdName());
		}

		channelPtr->sendDebug(msg, msg.dst);
	}


	void newCallerId(Cmsg & msg)
	{

		CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid"]);

		if (msg.isSet("CallerID"))//1.4
		{
			if (msg["CallerID"].str() == "<Unknown>")
				;//channelPtr->setCallerId("");
			else
				channelPtr->setCallerId(msg["CallerID"]);
		}

		if (msg.isSet("CallerIDName"))//1.4
		{
			if (msg["CallerIDName"].str() == "<Unknown>")
				;//channelPtr->setCallerId("");
			else
				channelPtr->setCallerIdName(msg["CallerIDName"]);
		}

		if (msg.isSet("CallerIDNum")) //1.8
		{
			if (msg["CallerIDNum"].str() == "<Unknown>")
				;//channelPtr->setCallerIdNum("");
			else
				channelPtr->setCallerId(msg["CallerIDNum"]);
		}

		channelPtr->sendDebug(msg, msg.dst);
	}


	//1.4
	SYNAPSE_REGISTER(ami_Event_Newcallerid)
	{
	// 	Event: Newcallerid
	// 	Privilege: call,all
	// 	Channel: SIP/605-00000033
	// 	CallerID: 605
	// 	CallerIDName: <Unknown>
	// 	Uniqueid: 1269866486.60
	// 	CID-CallingPres: 0 (Presentation Allowed, Not Screened)

	// Event: Newcallerid
	// Privilege: call,all
	// Channel: mISDN/0-u5
	// CallerID: 0622588835
	// CallerIDName: <Unknown>
	// Uniqueid: 1269866267.58
	// CID-CallingPres: 0 (Presentation Allowed, Not Screened)

	// Event: Newcallerid
	// Privilege: call,all
	// Channel: SIP/604-00000031
	// CallerID: 0858784323
	// CallerIDName: <Unknown>
	// Uniqueid: 1269866267.57
	// CID-CallingPres: 0 (Presentation Allowed, Not Screened)

		newCallerId(msg);
	}

	//1.8
	SYNAPSE_REGISTER(ami_Event_NewCallerid)
	{
		// |CallerIDName = hoofdlijn+31622588835 (string)
		// |CallerIDNum = +31622588835 (string)
		// |Channel = SIP/111-CDS_Datux-0000006f (string)
		// |Event = NewCallerid (string)
		// |Privilege = call,all (string)
		// |Uniqueid = 1395249879.117 (string)

		newCallerId(msg);

	}


	SYNAPSE_REGISTER(ami_Event_CEL)
	{
		// CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["UniqueID"]);
		// channelPtr->sendDebug(msg, msg.dst);

	}

	SYNAPSE_REGISTER(ami_Event_VarSet)
	{
		// CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid"]);
		// channelPtr->sendDebug(msg, msg.dst);
		;
	}

	SYNAPSE_REGISTER(ami_Event_ParkedCall)
	{
		;
	}

	// 0xdbfc30 SEND ami_Event_ExtensionStatus FROM 3:module@ami TO 6:module@asterisk (map:)
	//  |Context = ext-local (string)
	//  |Event = ExtensionStatus (string)
	//  |Exten = 100 (string)
	//  |Hint = SIP/100&Custom:DND100 (string)
	//  |Privilege = call,all (string)
	//  |Status = 1 (string)
	SYNAPSE_REGISTER(ami_Event_ExtensionStatus)
	{
		// CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["UniqueID"]);
		// channelPtr->sendDebug(msg, msg.dst);
		;
	}

	SYNAPSE_REGISTER(ami_Event_UserEvent)
	{
		// 0x7f4ee4001be0 RECV ami_Event_UserEvent FROM 3 BY 6:module@asterisk (map:)
		//  |Uniqueid = 1396725358.0 (string)
		//  |Event = UserEvent (string)
		//  |Owner = SIP/101 (string)
		//  |Privilege = user,all (string)
		//  |UserEvent = synapse_Parked (string)
		if (msg["UserEvent"]=="synapse_Parked")
		{
			CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid"]);
			CdevicePtr devicePtr=serverMan.getServerPtr(msg.dst)->getDevicePtr(msg["Owner"]);
			channelPtr->setOwnerPtr(devicePtr);
			channelPtr->sendDebug(msg, msg.dst);

		}
	}

	// 0x7f6168000900 SEND ami_Event_Hold FROM 3:module@ami TO 6:module@asterisk (map:)
	//  |Channel = SIP/101-0000002e (string)
	//  |Event = Hold (string)
	//  |Privilege = call,all (string)
	//  |Status = On (string)
	//  |Uniqueid = 1397749689.46 (string)
	SYNAPSE_REGISTER(ami_Event_Hold)
	{
		CchannelPtr channelPtr=serverMan.getServerPtr(msg.dst)->getChannelPtr(msg["Uniqueid"]);
		channelPtr->setOnHold(msg["Status"]=="On");
		channelPtr->sendDebug(msg, msg.dst);
	}

	//////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////// events to control this module, usually sent by webinterface to us.
	//////////////////////////////////////////////////////////////////////////////////////////

	/** Loads asterisk.conf and connects to specified servers.
	 * It will also handle server deletes/changes.
	 */
	SYNAPSE_REGISTER(asterisk_Config)
	{
		serverMan.reload("etc/synapse/asterisk.conf");
	}

	SYNAPSE_REGISTER(asterisk_GetStatus)
	{
		Cmsg out;
		out.event="asterisk_Status";
		out.dst=msg.src;
		out["status"]=serverMan.getStatus();
		out.send();
	}

	SYNAPSE_REGISTER(timer_Ready)
	{
		Cmsg out;
		out.clear();
		out.event="timer_Set";
		out["seconds"]=1;
		out["repeat"]=-1;
		out["dst"]=dst;
		out["event"]="asterisk_SendChanges";
		out.send();
	}




	SYNAPSE_REGISTER(asterisk_SendChanges)
	{
		serverMan.sendChanges();

	}

	SYNAPSE_REGISTER(asterisk_refresh)
	{
		serverMan.sendRefresh(msg.src);

	}


	/** Requests authentication for \c src

	\par Replys \c asterisk_authCall
		The session should authenticate by dialing a special crafted  number. After this asterisk knows to which device the client belongs, and can determine to which group the user belongs.
			\arg \c number The number the client should call to authenticate.

	\par Replys \c asterisk_authOk
		After the user called the specified number.
			\arg \c deviceId The deviceId the user was calling from.

	*/
	SYNAPSE_REGISTER(asterisk_authReq)
	{

		//try to reauthenticate with authcookie?
		if (msg["authCookie"]!="" && msg["deviceId"]!="" && msg["serverId"]!="")
		{
			//correct authcookie?
			if (serverMan.getAuthCookie(msg["serverId"], msg["deviceId"])==msg["authCookie"])
			{
				//authenticate session
				CserverPtr serverPtr=serverMan.getServerPtrByName(msg["serverId"]);
				serverMan.getSessionPtr(msg.src)->authorize(serverPtr, serverPtr->getDevicePtr(msg["deviceId"]));

				//session is re-authenticated, by authCookie
				Cmsg out;
				out.event="asterisk_authOk";
				out.dst=msg.src;
				out["deviceId"]=msg["deviceId"].str();
				out["serverId"]=msg["serverId"].str();
				out["authCookie"]=msg["authCookie"];
				out.send(0,Cmsg::INFO);

				return;
			}
		}

		//de-authenticate the user (e.g. logout)
		serverMan.getSessionPtr(msg.src)->deauthorize();

		//tell the client which number to call, to authenticate
		stringstream number;
		number << ASTERISK_AUTH << msg.src;

		Cmsg out;
		out.event="asterisk_reset"; //since we're logged out again, make sure the screen is cleared.
		out.dst=msg.src;
		out.send(0,Cmsg::INFO);

		out.event="asterisk_authCall";
		out["number"]=number.str();
		out.send(0,Cmsg::INFO);
	}



	//FIXME: when using multiple groups on one server, we need to do some extra validation to make sure someone isnt bridging to a channel in another group. (security)
	//(needs to be fixed by all actions, not just call)
	SYNAPSE_REGISTER(asterisk_Call)
	{

		CsessionPtr sessionPtr=serverMan.getSessionPtr(msg.src);
		CchannelPtr channelPtr;

		if (msg.isSet("reuseChannelId"))
			channelPtr=sessionPtr->getServerPtr()->getChannelPtr(msg["reuseChannelId"], false);

		sessionPtr->getServerPtr()->amiCall(sessionPtr->getDevicePtr(), channelPtr, msg["exten"], msg["parkLinked"]);

	}

	SYNAPSE_REGISTER(asterisk_Bridge)
	{

		CsessionPtr sessionPtr=serverMan.getSessionPtr(msg.src);
		CchannelPtr channel1Ptr;
		CchannelPtr channel2Ptr;

		if (msg.isSet("channel1"))
			channel1Ptr=sessionPtr->getServerPtr()->getChannelPtr(msg["channel1"], false);

		if (msg.isSet("channel2"))
			channel2Ptr=sessionPtr->getServerPtr()->getChannelPtr(msg["channel2"], false);

		sessionPtr->getServerPtr()->amiBridge(sessionPtr->getDevicePtr(), channel1Ptr, channel2Ptr, msg["parkLinked1"]);

	}


	SYNAPSE_REGISTER(asterisk_Park)
	{

		CsessionPtr sessionPtr=serverMan.getSessionPtr(msg.src);
		CchannelPtr channel1Ptr;
		channel1Ptr=sessionPtr->getServerPtr()->getChannelPtr(msg["channel1"], false);
		sessionPtr->getServerPtr()->amiPark(sessionPtr->getDevicePtr(), channel1Ptr);

	}

	SYNAPSE_REGISTER(asterisk_Hangup)
	{

		CsessionPtr sessionPtr=serverMan.getSessionPtr(msg.src);
		CchannelPtr channelPtr;

		channelPtr=sessionPtr->getServerPtr()->getChannelPtr(msg["channel"], false);
		sessionPtr->getServerPtr()->amiHangup(channelPtr);

	}

	//test raw commands
	//FIXME: remove, insecure, only use while debugging
	// SYNAPSE_REGISTER(asterisk_Test)
	// {
	//
	// 	CsessionPtr sessionPtr=serverMan.getSessionPtr(msg.src);
	//
	// 	Cmsg out;
	// 	out.clear();
	// 	out=msg;
	// 	out.src=sessionPtr->getServerPtr()->getSessionId();
	// 	out.event="ami_Action";
	// 	out.send();
	// }


}

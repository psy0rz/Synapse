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
#include <boost/regex.hpp>

using namespace std;

string gStatusStr;
string gErrorStr;
bool gOk;

synapse::Cconfig gConfig;

//active tweets that are shown on the bar
Cvar gTweets;

void setMarquee()
{
	Cmsg out;
	out.event="marquee_Set";
	out["classes"].list().push_back(string("twitter"));
	if (!gOk)
		out["text"]=gErrorStr;
	else
		out["text"]=gStatusStr;

	out.send();

}

SYNAPSE_REGISTER(module_Init)
{
	gOk=false;
	gConfig.load("etc/synapse/twitter_marquee.conf");
	
	Cmsg out;
	out.event="core_LoadModule";
  	out["name"]="marquee_m500";
  	out.send();
}

SYNAPSE_REGISTER(marquee_m500_Ready)
{
	Cmsg out;
	out.event="core_LoadModule";
  	out["name"]="twitter";
  	out.send();

}

SYNAPSE_REGISTER(twitter_Ready)
{
	Cmsg out;
	out.event="core_Ready";
  	out.send();
}



SYNAPSE_REGISTER(twitter_Ok)
{
	gOk=true;
	gErrorStr="";
	setMarquee();
}

SYNAPSE_REGISTER(twitter_Error)
{
	gOk=false;
	gErrorStr=msg["error"].str();
	setMarquee();
}

/*
 (map:)
 |delete = (map:)
 | |status = (map:)
 | | |id = 1.00807e+08 (long double)
 | | |id_str = 66497058894393344 (string)


0x2670870 SEND twitter_Data FROM 5:module@twitter TO broadcast (2:module@twitter_marquee 7:module@http_json ) (map:)
 |contributors = (empty)
 |coordinates = (empty)
 |created_at = Fri May 06 14:26:34 +0000 2011 (string)
 |entities = (map:)
 | |hashtags = (empty list)
 | |urls = (empty list)
 | |user_mentions = (list:)
 | | |(map:)
 | | | |id = 2.45697e+08 (long double)
 | | | |id_str = 245697242 (string)
 | | | |indices = (list:)
 | | | | |0 (long double)
 | | | | |15 (long double)
 | | | |name = Heleen (string)
 | | | |screen_name = heleenkerssies (string)
 |favorited = 0 (long double)
 |geo = (empty)
 |id = 6.04119e+08 (long double)
 |id_str = 66509162615545856 (string)
 |in_reply_to_screen_name = heleenkerssies (string)
 |in_reply_to_status_id = 1.99243e+09 (long double)
 |in_reply_to_status_id_str = 66477784972791808 (string)
 |in_reply_to_user_id = 2.45697e+08 (long double)
 |in_reply_to_user_id_str = 245697242 (string)
 |place = (empty)
 |retweet_count = 0 (long double)
 |retweeted = 0 (long double)
 |source = web (string)
 |text = @heleenkerssies hij doet het al :) (string)
 |truncated = 0 (long double)
 |user = (map:)
 | |contributors_enabled = 0 (long double)
 | |created_at = Tue Nov 16 23:18:31 +0000 2010 (string)
 | |default_profile = 1 (long double)
 | |default_profile_image = 1 (long double)
 | |description = (empty)
 | |favourites_count = 1 (long double)
 | |follow_request_sent = (empty)
 | |followers_count = 13 (long double)
 | |following = (empty)
 | |friends_count = 16 (long double)
 | |geo_enabled = 0 (long double)
 | |id = 2.1652e+08 (long double)
 | |id_str = 216519907 (string)
 | |is_translator = 0 (long double)
 | |lang = en (string)
 | |listed_count = 0 (long double)
 | |location = (empty)
 | |name = Edwin Eefting (string)
 | |notifications = (empty)
 | |profile_background_color = C0DEED (string)
 | |profile_background_image_url = http://a3.twimg.com/a/1303425044/images/themes/theme1/bg.png (string)
 | |profile_background_tile = 0 (long double)
 | |profile_image_url = http://a3.twimg.com/sticky/default_profile_images/default_profile_2_normal.png (string)
 | |profile_link_color = 0084B4 (string)
 | |profile_sidebar_border_color = C0DEED (string)
 | |profile_sidebar_fill_color = DDEEF6 (string)
 | |profile_text_color = 333333 (string)
 | |profile_use_background_image = 1 (long double)
 | |protected = 0 (long double)
 | |screen_name = EHEefting (string)
 | |show_all_inline_media = 0 (long double)
 | |statuses_count = 42 (long double)
 | |time_zone = (empty)
 | |url = (empty)
 | |utc_offset = (empty)
 | |verified = 0 (long double)



0x16b9de0 RECV twitter_Data FROM 5 BY 7:module@http_json (map:)
 |contributors = (empty)
 |coordinates = (empty)
 |created_at = Wed May 11 19:40:10 +0000 2011 (string)
 |favorited = 0 (long double)
 |geo = (empty)
 |id = -9.81324e+08 (long double)
 |id_str = 68400020382167040 (string)
 |in_reply_to_screen_name = (empty)
 |in_reply_to_status_id = (empty)
 |in_reply_to_status_id_str = (empty)
 |in_reply_to_user_id = (empty)
 |in_reply_to_user_id_str = (empty)
 |place = (empty)
 |retweet_count = 0 (long double)
 |retweeted = 0 (long double)
 |retweeted_status = (map:)
 | |contributors = (empty)
 | |coordinates = (empty)
 | |created_at = Wed May 11 17:33:38 +0000 2011 (string)
 | |favorited = 0 (long double)
 | |geo = (empty)
 | |id = 1.33393e+09 (long double)
 | |id_str = 68368179809890306 (string)
 | |in_reply_to_screen_name = (empty)
 | |in_reply_to_status_id = (empty)
 | |in_reply_to_status_id_str = (empty)
 | |in_reply_to_user_id = (empty)
 | |in_reply_to_user_id_str = (empty)
 | |place = (empty)
 | |retweet_count = 0 (long double)
 | |retweeted = 0 (long double)
 | |source = <a href="http://www.tweetdeck.com" rel="nofollow">TweetDeck</a> (string)
 | |text = De heren @stefankuhl en @edwingels namens Webba (www.webba.nl) opdracht gegeven om voor @RooBeekAdvies aan de slag te gaan. Succes heren! (string)
 | |truncated = 0 (long double)
 | |user = (map:)
 | | |contributors_enabled = 0 (long double)
 | | |created_at = Tue May 03 05:35:33 +0000 2011 (string)
 | | |default_profile = 1 (long double)
 | | |default_profile_image = 0 (long double)
 | | |description = Advies en interim / Ruimte en omgeving (ruimtelijke ordening; planologie) / Vergunning en Wabo (bouwen; vergunningmanagement) / Training en coachen   (string)
 | | |favourites_count = 0 (long double)
 | | |follow_request_sent = 0 (long double)
 | | |followers_count = 9 (long double)
 | | |following = 0 (long double)
 | | |friends_count = 0 (long double)
 | | |geo_enabled = 0 (long double)
 | | |id = 2.9215e+08 (long double)
 | | |id_str = 292149567 (string)
 | | |is_translator = 0 (long double)
 | | |lang = en (string)
 | | |listed_count = 0 (long double)
 | | |location = Zuidoost Drenthe (string)
 | | |name = RooBeek Advies (string)
 | | |notifications = 0 (long double)
 | | |profile_background_color = C0DEED (string)
 | | |profile_background_image_url = http://a3.twimg.com/images/themes/theme1/bg.png (string)
 | | |profile_background_tile = 0 (long double)
 | | |profile_image_url = http://a2.twimg.com/profile_images/1336791351/9928661_normal.jpg (string)
 | | |profile_link_color = 0084B4 (string)
 | | |profile_sidebar_border_color = C0DEED (string)
 | | |profile_sidebar_fill_color = DDEEF6 (string)
 | | |profile_text_color = 333333 (string)
 | | |profile_use_background_image = 1 (long double)
 | | |protected = 0 (long double)
 | | |screen_name = RooBeekAdvies (string)
 | | |show_all_inline_media = 0 (long double)
 | | |statuses_count = 1 (long double)
 | | |time_zone = (empty)
 | | |url = http://www.roobeek-advies.nl (string)
 | | |utc_offset = (empty)
 | | |verified = 0 (long double)
 |source = <a href="http://www.tweetdeck.com" rel="nofollow">TweetDeck</a> (string)
 |text = RT @RooBeekAdvies: De heren @stefankuhl en @edwingels namens Webba (www.webba.nl) opdracht gegeven om voor @RooBeekAdvies aan de slag te ... (string)
 |truncated = 1 (long double)
 |user = (map:)
 | |contributors_enabled = 0 (long double)
 | |created_at = Thu Aug 20 17:05:07 +0000 2009 (string)
 | |default_profile = 0 (long double)
 | |default_profile_image = 0 (long double)
 | |description = Webba, full service Internet & Reclamebureau (string)
 | |favourites_count = 0 (long double)
 | |follow_request_sent = 0 (long double)
 | |followers_count = 282 (long double)
 | |following = 0 (long double)
 | |friends_count = 160 (long double)
 | |geo_enabled = 0 (long double)
 | |id = 6.73626e+07 (long double)
 | |id_str = 67362552 (string)
 | |is_translator = 0 (long double)
 | |lang = en (string)
 | |listed_count = 12 (long double)
 | |location = Emmen (string)
 | |name = Webba_nl (string)
 | |notifications = 0 (long double)
 | |profile_background_color = 039ce3 (string)
 | |profile_background_image_url = http://a0.twimg.com/profile_background_images/169845957/ontwerp-achtergrond.jpg (string)
 | |profile_background_tile = 0 (long double)
 | |profile_image_url = http://a0.twimg.com/profile_images/1072739333/webba_normal.jpg (string)
 | |profile_link_color = 009de0 (string)
 | |profile_sidebar_border_color = 4b4a4d (string)
 | |profile_sidebar_fill_color = f2eded (string)
 | |profile_text_color = 575757 (string)
 | |profile_use_background_image = 1 (long double)
 | |protected = 0 (long double)
 | |screen_name = webba_nl (string)
 | |show_all_inline_media = 0 (long double)
 | |statuses_count = 778 (long double)
 | |time_zone = Amsterdam (string)
 | |url = http://www.webba.nl (string)
 | |utc_offset = 3600 (long double)
 | |verified = 0 (long double)


 */
SYNAPSE_REGISTER(twitter_Data)
{
	bool changed=false;

	//new twitter message
	if (msg.isSet("user"))
	{
		string name=msg["user"]["screen_name"];

		//not too old and a special user or has the twitbar hashtag
		if (	gConfig["show"].isSet(name) ||
				msg["text"].str().find(gConfig["keyword"].str())!=string::npos)
		{
			//add it
			gTweets.list().push_front(msg);

			//how many messages this user may do?
			int maxTweets=1;
			if (gConfig["show"].isSet(name))
				maxTweets=gConfig["show"][name];

			//count the messages for this user
			CvarList::iterator lastTweetI;
			int tweetCount=0;
			FOREACH_VARLIST_ITER( tweetI, gTweets)
			{
				if ((*tweetI)["user"]["screen_name"].str()==name)
				{
					lastTweetI=tweetI;
					tweetCount++;
				}
			}

			//too much? delete last one
			if (tweetCount>maxTweets)
				gTweets.list().erase(lastTweetI);

			//count the total number of keyword-messages
			tweetCount=0;
			FOREACH_VARLIST_ITER(tweetI, gTweets)
			{
				//not a regular user?
				if (!gConfig["show"].isSet( (*tweetI)["user"]["screen_name"] ))
				{
					//count it
					tweetCount++;
					lastTweetI=tweetI;
				}
			}

			//too much? delete last one
			if (tweetCount>gConfig["keyword_max_tweets"])
				gTweets.list().erase(lastTweetI);

			changed=true;
		}
	}

	//delete a message
	if (msg.isSet("delete"))
	{
		FOREACH_VARLIST_ITER(tweetI, gTweets)
		{
			//find the message
			if ((*tweetI)["id_str"]==msg["delete"]["status"]["id_str"])
			{
				//delete it
				gTweets.list().erase(tweetI);
				changed=true;
				break;
			}
		}
	}

	if (changed)
	{
		//now format a nice status string
		gStatusStr=gConfig["main_header"].str();
		FOREACH_VARLIST(tweet, gTweets)
		{
			string text;
			string name;
			if (tweet.isSet("retweeted_status"))
			{
				text=tweet["retweeted_status"]["text"].str();
				name=tweet["retweeted_status"]["user"]["screen_name"].str();
			}
			else
			{
				text=tweet["text"].str();
				name=tweet["user"]["screen_name"].str();
			}

			//filter stuff out we dont want
			FOREACH_VARMAP(replace, gConfig["replace"])
			{
				text=regex_replace(
						text,
						boost::regex(replace.first),
						replace.second.str(),
						boost::match_default | boost::format_all
				);
			}


			gStatusStr+=gConfig["name_header"].str();
			gStatusStr+=name;
			gStatusStr+=gConfig["text_header"].str();
			gStatusStr+=text;
		}

		gStatusStr+=gConfig["main_footer"].str();



		setMarquee();
	}

}



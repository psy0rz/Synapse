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


/*
 * cgroup.cpp
 *
 *  Created on: Jul 14, 2010

 */

#include "cgroup.h"
#include "cserver.h"
#include <string.h>
#include "boost/filesystem/fstream.hpp"
#include <boost/filesystem.hpp>

//groups: most times a tennant is considered a group.
//After authenticating, a session points to a device which in turn points to a group.'
//All devices of a specific tennant also point to this group
//events are only sent to Csessions that are member of the same group as the corresponding device.
namespace asterisk
{

    //for CSV file parsing. (move this into a seprate lib?)
    //from: http://stackoverflow.com/questions/1120140/how-can-i-read-and-parse-csv-files-in-c
    std::vector<std::string> parseNextLineCsvLine(std::istream& str, char seperator)
    {
        std::vector<std::string>   result;
        std::string                line;
        std::getline(str,line);

        std::stringstream          lineStream(line);
        std::string                cell;

        while(std::getline(lineStream,cell, seperator))
        {
            result.push_back(cell);
        }
        return result;
    }



	Cgroup::Cgroup(CserverMan * serverManPtr)
	{
		this->serverManPtr=serverManPtr;
	}

	void Cgroup::setId(string id)
	{
		this->id=id;
	}

    //loads and caches speeddial list (if it exists) and returns it
    //designed to parse csv exports from freepbx:
    // "full name";+313213123231;100
    Cvar Cgroup::getSpeedDial()
    {
        boost::filesystem::path csvFileName("etc/synapse/asterisk_speeddial_"+id+".csv");
        if (speedDialList.isEmpty())
        {
            //create list
            speedDialList.list();
            if (boost::filesystem::exists(csvFileName))
            {
                DEB("Loading speeddial for group '" << id << "', from file: " << csvFileName);
                ifstream csvStream;
                csvStream.exceptions( ifstream::badbit  );
                csvStream.open(csvFileName.string().c_str());
                while(!csvStream.eof())
                {
                    std::vector<std::string> fields;
                    fields=parseNextLineCsvLine(csvStream, ';');
                    if (fields.size()>=2)
                    {
                        Cvar addressEntry;
                        addressEntry["full_name"]=fields[0];
                        addressEntry["number"]=fields[1];
                        if (fields.size()>=3)
                        {
                            addressEntry["category"]=fields[2];
                        }
                        else
                        {
                            addressEntry["category"]=""; //will end up in phone overview instead of seperate list
                        }
                        speedDialList.list().push_back(addressEntry);
                    }
                }
                csvStream.close();


            }
            else
            {
                WARNING("Not loading speeddial for group '" << id << "', because file does not exist: " << csvFileName);
            }
        }

        return(speedDialList);
    }

    //loads phonebook into phoneBookList (if it exists)
    //format:
    // company;first_name;last_name;number
    Cvar Cgroup::getPhoneBook()
    {
        boost::filesystem::path csvFileName("etc/synapse/asterisk_phonebook_"+id+".csv");
        if (phoneBookList.isEmpty())
        {
            //create list
            phoneBookList.list();
            if (boost::filesystem::exists(csvFileName))
            {
                DEB("Loading phonebook for group '" << id << "', from file: " << csvFileName);
                ifstream csvStream;
                csvStream.exceptions( ifstream::badbit  );
                csvStream.open(csvFileName.string().c_str());
                while(!csvStream.eof())
                {
                    std::vector<std::string> fields;
                    fields=parseNextLineCsvLine(csvStream, ';');
                    if (fields.size()>=4)
                    {
                        Cvar addressEntry;
                        addressEntry["company"]=fields[0];
                        addressEntry["first_name"]=fields[1];
                        addressEntry["last_name"]=fields[2];
                        addressEntry["number"]=fields[3];
                        phoneBookList.list().push_back(addressEntry);
                    }
                }
                csvStream.close();
            }
            else
            {
                WARNING("Not loading phonebook for group '" << id << "', because file does not exist: " << csvFileName);
            }
        }
        return(phoneBookList);
    }
    //
    // //regex query on all fields in phonebook list
    // void Cgroup::searchPhoneBook(string re)
    // {
    //
    // }

	string Cgroup::getId()
	{
		return(id);
	}

    void Cgroup::sendRefresh(int dst)
	{
        Cmsg out;
        out.dst=dst;
        out.event="asterisk_speedDialList";
        out["numbers"]=getSpeedDial();
        out["groupId"]=getId();
        send(out);

        out.clear();
        out.dst=dst;
        out.event="asterisk_phoneBookList";
        out["numbers"]=getPhoneBook();
        out["groupId"]=getId();
        send(out);


	}


	void Cgroup::send(Cmsg & msg)
	{
		serverManPtr->send(id, msg);

	}

	string Cgroup::getStatus(string prefix)
	{
		return (
			prefix+"Group "+id
		);
	}



}

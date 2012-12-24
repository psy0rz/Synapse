#include "utils.h"
#include "boost/filesystem/fstream.hpp"
#include "boost/regex.hpp"


namespace utils
{
	using namespace boost::posix_time;
	using boost::filesystem::ofstream;
	using boost::filesystem::ifstream;

	//replace a bunch of regular expressions in a file.
	//the regex Cvar is a hasharray:
	// regex => replacement
	void regexReplaceFile(const string & inFilename, const string & outFilename,  synapse::CvarMap & regex)
	{
		//read input
		stringbuf inBuf;
		ifstream inStream;
		inStream.exceptions (  ifstream::failbit| ifstream::badbit );
		inStream.open(inFilename);
		inStream.get(inBuf,'\0');
		inStream.close();

		//build regex and formatter
		string regexStr;
		stringstream formatStr;
		int count=1;
		for(synapse::CvarMap::iterator I=regex.begin(); I!=regex.end(); I++)
		{
			if (count>1)
				regexStr+="|";

			regexStr+="("+ I->first +")";
			formatStr << "(?{" << count << "}" << I->second.str() << ")";
			count++;
		}

		//apply regexs
		string outBuf;
		outBuf=regex_replace(
				inBuf.str(),
				boost::regex(regexStr),
				formatStr.str(),
				boost::match_default | boost::format_all
		);

		//write output
		ofstream outStream;
		outStream.exceptions ( ofstream::eofbit | ofstream::failbit | ofstream::badbit );
		outStream.open(outFilename);
		outStream << outBuf;
		outStream.close();
	}

	//return a random readable string, for use as key or uniq id
 	struct drand48_data gRandomBuffer;
 	bool gRandomBufferReady=false;

	string randomStr(int length)
	{
		if (!gRandomBufferReady)
		{
			gRandomBufferReady=true;
			srand48_r(time(NULL), &gRandomBuffer);
		}

		long int r;
		string chars("abcdefghijklmnopqrstuvwxyz0123456789");
		string s;
		for (int i=0; i<length; i++)
		{
			mrand48_r(&gRandomBuffer, &r);
			s=s+chars[abs(r) % chars.length()];
		}
		return(s);
	}


}

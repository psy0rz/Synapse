#include "cexception.h"
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

namespace synapse
{
	runtime_error::runtime_error(const std::string &err )
	:std::runtime_error(err)
	{
		#ifdef NDEBUG
			mTrace="Please enable debugging to get exception stack traces.";
		#else
				int j, nptrs;
				const int buffer_size=100;
				void *buffer[buffer_size];
				char **strings;

				nptrs = backtrace(buffer, buffer_size);

				strings = backtrace_symbols(buffer, nptrs);
				if (strings == NULL)
				{
					mTrace="Error while getting backtrace.";
					return;
				}

				for (j = 0; j < nptrs; j++)
				{
					mTrace+=strings[j];
					mTrace+="\n";
				}

				free(strings);

		#endif
	}

	const char * runtime_error::getTrace() const throw()
	{
		return(mTrace.c_str());
	}

	runtime_error::~runtime_error() throw()
	{

	}

}
 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cmessageman.h"

int main(int argc, char *argv[])
{
	CmessageMan messageMan;
	if (argc==2)
	{
		return (messageMan.run("modules/core.module/libcore.so",argv[1]));
	}
	else
	{
		printf("Usage: ./synapse <initialmodule.so>\n"); 
		return (1);
	}
}

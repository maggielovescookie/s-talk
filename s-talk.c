#include "communication.h"
#include <stdio.h>

int main(int argCount, char** args)
{
	if(argCount != 4)
	{
		printf("Input error: expected 4 arguments\n");
		return -1;
	}

	int myPort = atoi(args[1]);
	char* remoteMachineName = args[2];
	int remotePort = atoi(args[3]);

	if(Talk_init(myPort, remoteMachineName, remotePort) != 0)
	{
		return -1;
	}

	Talk_start();

	Talk_shutdown();

	return 0;
}

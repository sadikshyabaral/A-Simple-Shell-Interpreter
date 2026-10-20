#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
	const char* tag = argv[1];
	int interval = atoi(argv[2]);

	if (argc != 3)
	{
		fprintf(stderr, "Usage: inf tag interval\n");
	}
	else
	{
		while(1)
		{
			fprintf(stdout, "%s\n", tag);
			sleep(interval);
		}
	}
	return 0;
}

#include <stdio.h>
#include <stdlib.h>

#include "npk.h"

#define APP_NAME "unnpk"
#define APP_VERSION "0.0.1jinx1"
#define APP_DEVELOPER "karin"
#define APP_RELEASE "2017.12.06"

/*
* https://github.com/YJBeetle/unnpk
*/

int main(int argc, char *argv[])
{
	if (argc == 1)
	{
		printf("%s is a extractor for npk file of NetEase games (Win32).\n", APP_NAME);
		printf("  %s %s@%s\n", APP_VERSION, APP_DEVELOPER, APP_RELEASE);
		printf("USAGE: unnpk <npk file path ...> <output directory path>\n");
		exit(EXIT_SUCCESS);
	}

	enable(NPK_DEBUG);

	int end = argc == 2 ? argc : argc - 1;

	int i;
	for (i = 1; i < end; i++)
	{
		printf("********************** [ %s -> Start ] **********************\n", argv[i]);
		if (unnpk(argv[i], argc == 2 ? "." : argv[argc - 1]))
			printf("********************** [ DONE! ] **********************\n\n");
		else
			printf("********************** [ FAIL! ] **********************\n\n");
	}

	return 0;
}

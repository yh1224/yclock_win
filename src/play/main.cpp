#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "../yclock/voice.h"

void
main(int argc, char *argv[])
{
	playVoice(argv[1], 1, 100);
	exit(EXIT_SUCCESS);
}

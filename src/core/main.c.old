/*
          ________________________
        /               /XXXX/  / |
       /               /----/  /  |
      /               /----/  /  /|
     /               /----/  /  / /
    /               /XXXX/  /  / /
   /________________|XXXX|_/  / /
   | | pNES         |XXXX| | / /
   | |______________|XXXX| |/ /
   |________________|XXXX|_| /
   \ o [  ][  ]     |[][]| |/
    \_______________|XXXX|_/

	Matt Penny 2017
*/

#include <errno.h>
#include <stdio.h>

#include "renderer.h"
#include "nes.h"

int main(int argc, char** argv)
{
	NES nes;
	SDL_Event se;

	RenderSurface screen;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

	if (renderer_init_surface(&screen, "pNES") != 0)
	{
		fprintf(stderr, "Error: unable to initialize render surface (code %d)\n", errno);
		return -1;
	}

	nes_init(&nes);
	if (nes_load_rom(&nes, argv[1]) != 0)
	{
		fprintf(stderr, "Error: unable to load ROM\n");
		return 1;
	}

	/* Here we go! */
	for (;;)
	{
		nes_update(&nes, screen);
		
		SDL_PollEvent(&se); /* TODO: thread this */
		if (se.type == SDL_QUIT)
			break;
	}

	SDL_Quit();

	return 0;
}
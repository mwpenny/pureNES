/*
          ________________________
        /               /MPMP/  / |
       /               /----/  /  |
      /               /----/  /  /|
     /               /----/  /  / /
    /               /MPMP/  /  / /
   /________________|MPMP|_/  / /
   | | pNES         |MPMP| | / /
   | |______________|MPMP| |/ /
   |________________|MPMP|_| /
   \ o [  ][  ]     |[][]| |/
    \_______________|MPMP|_/

	Matt Penny 2015-2016
*/

#include "renderer.h"
#include "nes.h"
#include "apu.h"

int main(int argc, char** argv)
{
	NES nes;
	SDL_Event se;

	RenderSurface screen;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

	renderer_init_surface(&screen, "pNES");

	nes_init(&nes);
	nes_load_rom(&nes, argv[1]);

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
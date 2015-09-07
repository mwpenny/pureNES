/* pNES */

#include <SDL.h>
#include "nes.h"

int main(int argc, char** argv)
{
	NES nes;
	SDL_Event se;
	SDL_Surface* screen = SDL_SetVideoMode(800, 600, 32, SDL_HWSURFACE); /* use SDL_HWSURFACE? */
	SDL_WM_SetCaption("pNES", "pNES");
	SDL_Init(SDL_INIT_EVERYTHING);

	nes_init(&nes);
	nes_load_rom(&nes, argv[1]);

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
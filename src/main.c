/* pNES */

#include "renderer.h"
#include "nes.h"

int main(int argc, char** argv)
{
	NES nes;
	SDL_Event se;

	RenderSurface screen;
	renderer_init_surface(&screen, "pNES");

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
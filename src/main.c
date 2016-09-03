/* pNES */

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

	/*apu_genwave();*/
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
#include <string.h>
#include <stdlib.h>

#include "nes.h"
#include "game.h"

#include "controller.h"
#include "controllers/keyboard.h"

void nes_init(NES* nes)
{
	/* Initialize the system */
	memset(nes->ram, 0, RAMSIZE);
	cpu_init(&nes->cpu, nes);
	ppu_init(&nes->ppu, nes);
	controller_kb_init(&nes->c1);
	controller_kb_init(&nes->c2);
}

void nes_load_rom(NES* nes, char* path)
{
	game_load(&nes->game, path);

	/* Reset (start) system */
	cpu_interrupt(&nes->cpu, INT_RST);
}

void nes_update(NES* nes, SDL_Surface* screen)
{
	int cycles = cpu_step(&nes->cpu), i = 0;
	for (; i < cycles*3; ++i)
		ppu_step(&nes->ppu, screen);
		/*ppu_render_pattern_table(&nes->ppu, screen);*/
		/*ppu_render_nametable(&nes->ppu, screen);*/

	/* TODO: move this out of emulator (make emu a DLL and update controllers in front-end?) */
	controller_update(&nes->c1);
	/*controller_update(&nes->c2);*/
}
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
	apu_init(&nes->apu);
	controller_kb_init(&nes->c1);
	controller_kb_init(&nes->c2);
}

void nes_load_rom(NES* nes, char* path)
{
	game_load(&nes->game, path);

	/* Start system */
	cpu_power(&nes->cpu);
}

void nes_update(NES* nes, SDL_Surface* screen)
{
	int ppu_cycles, i = 0;
	if (nes->cpu.is_running)
		ppu_cycles = cpu_step(&nes->cpu)*3;
	else
		ppu_cycles = 1;

	for (; i < ppu_cycles; ++i)
		ppu_step(&nes->ppu, screen);

	for (i = 0; i < ppu_cycles/3; ++i)
		apu_tick(&nes->apu);

	/* TODO: move this out of emulator (make a front-end which links to the emu core) */
	controller_update(&nes->c1);
	/*controller_update(&nes->c2);*/
}
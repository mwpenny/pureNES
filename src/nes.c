#include <string.h>
#include <stdlib.h>

#include "cartridge.h"
#include "nes.h"

#include "controller.h"
#include "controllers/keyboard.h"

void nes_init(NES* nes)
{
	/* Initialize the system */
	memset(nes->ram, 0, RAMSIZE);
	cpu_init(&nes->cpu, nes);
	ppu_init(&nes->ppu, nes);
	apu_init(&nes->apu, nes);
	controller_kb_init(&nes->c1);
	controller_kb_init(&nes->c2);
}

void nes_load_rom(NES* nes, char* path)
{
	cartridge_load(&nes->cartridge, path);

	/* Start system */
	cpu_power(&nes->cpu);
}

void nes_update(NES* nes, SDL_Surface* screen)
{
	int cycles = 0, i = 0;
	if (nes->cpu.is_running)
		cycles = cpu_step(&nes->cpu);

	/* TODO: do APU and PPU still run when the CPU is halted? */
	for (i = 0; i < cycles; ++i)
		apu_tick(&nes->apu);

	for (i = 0; i < cycles*3; ++i)
		ppu_tick(&nes->ppu, screen);

	/* TODO: move this out of emulator (make a front-end which links to the emu core) */
	controller_update(&nes->c1);
	/*controller_update(&nes->c2);*/
}
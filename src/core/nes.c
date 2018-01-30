#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "cartridge.h"
#include "nes.h"

void nes_init(NES* nes, NESInitInfo* init_info)
{
	/* Initialize the system */
	memset(nes, 0, sizeof(*nes));

	/* TODO: move this */
	SDL_Init(SDL_INIT_AUDIO);

	memset(nes->ram, 0, RAMSIZE);
	cpu_init(&nes->cpu, nes);
	ppu_init(&nes->ppu, nes, init_info);
	apu_init(&nes->apu, nes);
	controller_init(&nes->c1);
	controller_init(&nes->c2);
}

int nes_load_rom(NES* nes, char* path)
{
	if (cartridge_load(&nes->cartridge, path) != 0)
		return -1;

	/* Start system */
	cpu_power(&nes->cpu);
	return 0;
}

int nes_update(NES* nes)
{
	uint16_t cycles = 0, i = 0;
	if (nes->cpu.is_running)
		cycles = cpu_step(&nes->cpu);

	/* TODO: do APU and PPU still run when the CPU is halted? */
	for (i = 0; i < cycles; ++i)
		apu_tick(&nes->apu);

	for (i = 0; i < cycles*3; ++i)
		ppu_tick(&nes->ppu);

	controller_update(&nes->c1);
	controller_update(&nes->c2);
	return cycles;
}

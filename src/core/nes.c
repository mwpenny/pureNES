#include <stdlib.h>
#include <string.h>

#include "cartridge.h"
#include "nes.h"

void nes_init(NES* nes, NESInitInfo* init_info)
{
	/* Initialize the system */
	memset(nes, 0, sizeof(*nes));

	memset(nes->ram, 0, RAMSIZE);
	cpu_init(&nes->cpu, nes);
	ppu_init(&nes->ppu, nes, init_info);
	apu_init(&nes->apu, nes, init_info);
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

void nes_unload_rom(NES* nes)
{
	cartridge_unload(&nes->cartridge);
	nes->cpu.is_running = 0;
}

void nes_cleanup(NES* nes)
{
	apu_cleanup(&nes->apu);
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

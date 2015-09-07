#include <string.h>
#include <stdlib.h>

#include "nes.h"
#include "rom.h"

void nes_init(NES* nes)
{
	/* Initialize the system */
	nes->prg1 = nes->prg2 = NULL;
	memset(nes->ram, 0, RAMSIZE);
	cpu_init(&nes->cpu, nes);
	ppu_init(&nes->ppu, nes);
}

void nes_load_rom(NES* nes, char* path)
{
	ROM_Header head;
	FILE* rom = fopen(path, "rb");

	rom_parse(rom, &head);

	/* Get and load rom bank(s) */
	nes->prg1 = (uint8_t*)malloc(0x4000*head.rom_banks*sizeof(uint8_t));
	fread(nes->prg1, 1, 0x4000*head.rom_banks*sizeof(uint8_t), rom);

	if (head.rom_banks == 1) /* mirror bank 1 if there is no bank 2 */
		nes->prg2 = nes->prg1;
	else
		nes->prg1 = nes->prg1 + 0x4000;

	nes->vram = (uint8_t*)malloc(0x2000*head.vrom_banks*sizeof(uint8_t));
	fread(nes->vram, 1, 0x2000*head.vrom_banks*sizeof(uint8_t), rom);

	fclose(rom);

	/* Reset (start) system */
	cpu_interrupt(&nes->cpu, INT_RST);
}

void nes_update(NES* nes, SDL_Surface* screen)
{
	int cycles = cpu_tick(&nes->cpu), i = 0;
	for (; i < cycles*3; ++i)
		ppu_tick(&nes->ppu, screen);
}
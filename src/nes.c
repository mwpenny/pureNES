#include <string.h>
#include <stdlib.h>

#include "nes.h"
#include "rom.h"

#include "controller.h"
#include "controllers/keyboard.h"

void nes_init(NES* nes)
{
	/* Initialize the system */
	nes->prg1 = nes->prg2 = NULL;
	memset(nes->ram, 0, RAMSIZE);
	cpu_init(&nes->cpu, nes);
	ppu_init(&nes->ppu, nes);
	controller_kb_init(&nes->c1);
}

void nes_load_rom(NES* nes, char* path)
{
	ROM_Header head;
	FILE* rom = fopen(path, "rb");
	uint8_t* rbanks;

	rom_parse(rom, &head);
	/* TODO: ERROR CHECKING */

	/* Get and load rom bank(s) */
	rbanks = (uint8_t*)malloc(0x4000*head.rom_banks*sizeof(uint8_t));
	fread(rbanks, 1, 0x4000*head.rom_banks*sizeof(uint8_t), rom);

	nes->prg1 = rbanks;

	if (head.rom_banks == 1) /* mirror bank 1 if there is no bank 2 */
		nes->prg2 = rbanks;
	else
		nes->prg2 = nes->prg1 + 0x4000;

	nes->vrom = (uint8_t*)malloc(0x2000*head.vrom_banks*sizeof(uint8_t));
	fread(nes->vrom, 1, 0x2000*head.vrom_banks*sizeof(uint8_t), rom);

	fclose(rom);

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
}
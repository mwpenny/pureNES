/* rom.c */
/* iNES ROM parser */

#include <stdlib.h>
#include "cpu.h"
#include "ppu.h"
#include "rom.h"
#include "mappers.h"
#include <SDL.h>

int rom_parse(FILE* rom, ROM_Header* header)
{
	/* Get ROM file header and check validity */
	int read = fread(header, 1, sizeof(ROM_Header), rom); /* UNSAFE */
	return (read == sizeof(ROM_Header) && !memcmp("NES\x1A", header->magic, 4));
}

int main(int argc, char** argv)
{
	int i;
	ROM_Header head;
	CPU cpu;
	PPU ppu;
	Memory mem;
	unsigned char* rBanks;
	unsigned char* vrBanks;
	FILE* rom = fopen(argv[1], "rb");
	FILE* log = fopen("cpu.log", "w");

	SDL_Event se;
	SDL_Surface* screen = SDL_SetVideoMode(800, 600, 32, SDL_HWSURFACE); /* use SDL_HWSURFACE? */
	SDL_WM_SetCaption("pNES", "pNES");

	if (!rom)
	{
		puts("[-] Error: could not open ROM file.");
		return 1;
	}

	if (argc < 1 || !rom_parse(rom, &head))
	{
		puts("[-] Error: could not parse ROM header.");
		return 1;
	}

	printf("File: %s\n", argv[1]);
	printf("ROM size: %dkB\n", head.rom_banks*16);
	printf("VROM size: %dkB\n", head.vrom_banks*8);

	printf("Mapper: %s\n", mappers[(head.flags1 >> 4) | (head.flags2 & 0xF0)]);

	printf("4-screen VRAM: %s\n", (head.flags1 & 8) ? "YES" : "NO");
	printf("Trainer: %s\n", (head.flags1 & 4) ? "YES" : "NO");
	printf("Battery: %s\n", (head.flags1 & 2) ? "YES" : "NO");
	printf("Mirroring: %s\n", (head.flags1 & 1) ? "VERTICAL" : "HORIZONTAL");

	printf("VS Unisystem: %s\n", (head.flags2 & 1) ? "YES" : "NO");
	printf("PlayChoice-10: %s\n", (head.flags2 & 2) ? "YES" : "NO");
		
	printf("Color: %s\n", (head.flags3 & 1) ? "PAL" : "NTSC");

	/* map PRG ROM into memory */
	/* TODO: account for trainer? */
	rBanks = (unsigned char*)malloc(16*head.rom_banks*1024*sizeof(uint8_t));
	fread(rBanks, 1, 16*head.rom_banks*1024*sizeof(uint8_t), rom);

	vrBanks = (unsigned char*)malloc(8*head.vrom_banks*1024*sizeof(uint8_t));
	fread(vrBanks, 1, 8*head.vrom_banks*1024*sizeof(uint8_t), rom);
	ppu.vram = vrBanks;

	fclose(rom);

	cpu_init(&cpu, &mem);
	memory_map(&mem.ram, cpu.ram);
	memory_map(&mem.ppu_reg, (uint8_t*)&ppu); /* TODO: LIKELY UNSAFE */

	/* map PRG-ROM banks */
	memory_map(&mem.prg1, rBanks);
	if (head.rom_banks == 1) /* mirror bank 1 if there is no bank 2 */
		memory_map(&mem.prg2, rBanks);
	else 
		memory_map(&mem.prg1, rBanks + 0x4000);

	puts("\nPress enter to start CPU emulation");
	/*scanf("%c",&i);*/

	cpu_reset(&cpu);

	SDL_Init(SDL_INIT_EVERYTHING);
	ppu_render_pattern_table(&ppu, screen);
	

	/* CPU update loop */
	for (;;)
	{
		cpu_tick(&cpu, log);
		/* scanf("%c",&i); */
		SDL_PollEvent(&se); /* TODO: thread this */
		if (se.type == SDL_QUIT)
			break;
/*		ppu_render_pattern_table(&ppu, screen);*/
	}

	SDL_Quit();

	fclose(log);
	free(rBanks);
	return 0;
}
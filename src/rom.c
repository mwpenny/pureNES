/* rom.c */
/* iNES ROM parser */

#include <stdlib.h>
#include "cpu.h"
#include "rom.h"
#include "mappers.h"

int rom_parse(FILE* rom, ROM_Header* header)
{
	/* Get ROM file header and check validity */
	int read = fread(header, 1, sizeof(ROM_Header), rom);
	return (read == sizeof(ROM_Header) && !memcmp("NES\x1A", header->magic, 4));
}

int main(int argc, char** argv)
{
	int i;
	ROM_Header head;
	CPU cpu;
	unsigned char* rBanks;
	FILE* rom = fopen(argv[1], "rb");
	FILE* log = fopen("cpu.log", "w");

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
	printf("ROM size: %dkB\n", head.rom_size*16);
	printf("VROM size: %dkB\n", head.vrom_size*8);

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
	rBanks = (unsigned char*)malloc(16*head.rom_size*1024*sizeof(uint8_t));
	i = fread(rBanks, 1, 16*head.rom_size*1024*sizeof(uint8_t), rom);
	fclose(rom);
	memory_map_rom(rBanks, 16*head.rom_size*1024*sizeof(uint8_t));

	puts("\nPress enter to start CPU emulation");
	scanf("%c",&i);

	cpu_reset(&cpu);

	/* CPU update loop */
	for (;;)
	{
		cpu_tick(&cpu, log);
		/* scanf("%c",&i); */
	}

	fclose(log);
	free(rBanks);
	return 0;
}
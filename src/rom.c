/* rom.c */
/* iNES ROM parser */

#include "rom.h"
#include "mappers.h"

int rom_parse(char* path, ROM_Header* header)
{
	/* Get ROM file header and check validity */
	FILE* rom = fopen(path, "rb");
	int read = fread(header, 1, sizeof(ROM_Header), rom);
	fclose(rom);
	return (read == sizeof(ROM_Header) && !memcmp("NES\x1A", header->magic, 4));
}

int main(int argc, char** argv)
{
	ROM_Header head;

	if (argc < 1 || !rom_parse(argv[1], &head))
		puts("Could not load ROM.");

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

	return 0;
}
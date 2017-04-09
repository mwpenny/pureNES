/* Reads in NES ROM files (iNES ROMS only, for now) */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "cartridge.h"
#include "mapper.h"

/* iNES header format:
	0-3: "NES"<EOF>
	  4: Number of 16KB PRG ROM pages
	  5: Number of 8KB CHR ROM pages (0 indicates CHR RAM)
	  6: Flags 6
		  0: Mirroring (0=horizontal, 1=vertical)
		  1: Battery-backed RAM at ($6000-$7FFF)
		  2: Trainer present after header (loaded at $7000-$71FF)
		  3: Four screen mirroring (ignore bit 0)
	    4-7: Lower nibble of mapper number
	  7: Flags 7
		  0: VS System cartridge
		  1: Playchoice 10 cartridge
	    2-3: Use INESv2 if set to binary 10, otherwise should be 0
	    4-7: Upper nibble of mapper number
	  8: Number of 8KB PRG RAM pages (0 indicates 1 page, for compatibility)
	  9: Flags 9
		  0: TV system (0=NTSC, 1=PAL)
	    2-7: Reserved (should be 0)
  10-15: Reserved (should be 0) */
void cartridge_load(Cartridge* cart, char* path)
{
	FILE* rom;
	uint8_t header[16];
	uint8_t mapper_num;
	uint8_t rom_start_ofs = 0;  /* From header */
	long file_size;
	memset(cart, 0, sizeof(*cart));

	if (!(rom = fopen(path, "rb")) || fseek(rom, 0, SEEK_END) != 0)
	{
		fprintf(stderr, "Error: unable to open ROM file (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if ((file_size = ftell(rom)) < 0x4010)
	{
		fprintf(stderr, "Error: input file too small\n");
		exit(EXIT_FAILURE);
	}
	if (fseek(rom, 0, SEEK_SET) != 0 || fread(header, 1, 16, rom) != 16)
	{
		fprintf(stderr, "Error: unable to read ROM header (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* First 4 bytes of header are "NES" + EOF */
	if (memcmp(header, "NES\x1A", 4) != 0)
	{
		fprintf(stderr, "Error: invalid ROM image\n");
		exit(EXIT_FAILURE);
	}

	/* Parse header */
	cart->prg_rom_size = 0x4000 * header[4];
	cart->chr_size = 0x2000 * ((header[5] > 0) ? header[5] : 1);
	cart->prg_ram_size = 0x2000 * ((header[8] > 0) ? header[8] : 1);
	cart->has_nvram = header[6] & 0x02;
	mapper_num = ((header[6] & 0xF0) >> 4) | (header[7] & 0xF0);

	if (header[6] & 0x08)
		cart->mirror_mode = MIRRORING_4SCREEN;
	else if (header[6] & 0x01)
		cart->mirror_mode = MIRRORING_VERTICAL;
	else
		cart->mirror_mode = MIRRORING_HORIZONTAL;

	if (header[9] & 0x01)
		cart->video_mode = VIDEO_PAL;
	else
		cart->video_mode = VIDEO_NTSC;

	/* Load ROM */
	if (!(cart->prg_rom = (uint8_t*)malloc(cart->prg_rom_size)))
	{
		fprintf(stderr, "Error: unable to allocate memory for PRG ROM (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (!(cart->chr = (uint8_t*)malloc(cart->chr_size)))
	{
		fprintf(stderr, "Error: unable to allocate CHR memory (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (!(cart->prg_ram = (uint8_t*)malloc(cart->prg_ram_size)))
	{
		fprintf(stderr, "Error: unable to allocate memory for PRG RAM (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	rom_start_ofs = (header[6] & 0x04) ? 0x200 : 0;  /* Skip over trainer */
	if (fseek(rom, rom_start_ofs, SEEK_CUR) != 0 || fread(cart->prg_rom, 1, cart->prg_rom_size, rom) != cart->prg_rom_size)
	{
		fprintf(stderr, "Error: unable to read PRG ROM (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (header[5] && fread(cart->chr, 1, cart->chr_size, rom) != cart->chr_size)
	{
		fprintf(stderr, "Error: unable to read CHR ROM (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	fclose(rom);
	mapper_init(&cart->mapper, (struct Cartridge*)cart, mapper_num);
}

void cartridge_unload(Cartridge* cart)
{
	free(cart->prg_rom);
	free(cart->prg_ram);
	free(cart->chr);
	mapper_cleanup(&cart->mapper);
}

uint8_t cartridge_read(Cartridge* cart, uint16_t addr)
{
	if (addr < 0x8000)
		return *mapper_get_banked_mem(&cart->mapper.prg_ram_banks, addr);
	return *mapper_get_banked_mem(&cart->mapper.prg_rom_banks, addr - 0x8000);
}

void cartridge_write(Cartridge* cart, uint16_t addr, uint8_t val)
{
	if (addr < 0x8000)
		*mapper_get_banked_mem(&cart->mapper.prg_ram_banks, addr) = val;
	else
		cart->mapper.write(&cart->mapper, addr, val);
}

/* Reads in NES ROM files (iNES ROMS only, for now) */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "cartridge.h"
#include "mapper.h"

#define max(x, y) ((x) > (y) ? (x) : (y))

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
	    2-3: Use INESv2 if set to 0b10, otherwise should be 0
	    4-7: Upper nibble of mapper number
	  8: Number of 8KB PRG RAM pages (0 indicates 1 page, for compatibility)
	  9: Flags 9
		  0: TV system (0=NTSC, 1=PAL)
	    2-7: Reserved (should be 0)
  10-15: Reserved (should be 0) */
int cartridge_load(Cartridge* cart, char* path)
{
	FILE* rom;
	uint8_t header[16];
	uint8_t mapper_num;
	uint16_t rom_start_ofs = 0;
	long file_size;
	memset(cart, 0, sizeof(Cartridge));

	if (!(rom = fopen(path, "rb")))
	{
		fprintf(stderr, "Error: unable to open ROM file (code %d)\n", errno);
		return -1;
	}
	if (fseek(rom, 0, SEEK_END) != 0 || (file_size = ftell(rom)) < 0x4010)
	{
		fprintf(stderr, "Error: input file too small\n");
		fclose(rom);
		return -1;
	}
	if (fseek(rom, 0, SEEK_SET) != 0 || fread(header, 1, 16, rom) != 16)
	{
		fprintf(stderr, "Error: unable to read ROM header (code %d)\n", errno);
		fclose(rom);
		return -1;
	}

	/* First 4 bytes of header are "NES" + EOF */
	if (memcmp(header, "NES\x1A", 4) != 0)
	{
		fprintf(stderr, "Error: invalid ROM image\n");
		fclose(rom);
		return -1;
	}

	/* Parse header */
	cart->prg_rom.size = 0x4000 * header[4];
	cart->chr.size = 0x2000 * max(header[5], 1);
	cart->prg_ram.size = 0x2000 * max(header[8], 1);
	cart->has_nvram = (header[6] >> 1) & 1;
	mapper_num = ((header[6] & 0xF0) >> 4) | (header[7] & 0xF0);

	if (header[6] & 0x08)
		cart->mirror_mode = MIRRORING_4SCREEN;
	else if (header[6] & 0x01)
		cart->mirror_mode = MIRRORING_VERTICAL;
	else
		cart->mirror_mode = MIRRORING_HORIZONTAL;
	rom_start_ofs = (header[6] & 0x04) ? 0x200 : 0;  /* Skip over trainer */

	if (header[9] & 0x01)
		cart->video_mode = VIDEO_PAL;
	else
		cart->video_mode = VIDEO_NTSC;

	/* Load ROM */
	if (!(cart->prg_rom.data = (uint8_t*)malloc(cart->prg_rom.size)))
	{
		fprintf(stderr, "Error: unable to allocate memory for PRG ROM (code %d)\n", errno);
		fclose(rom);
		return -1;
	}
	if (!(cart->chr.data = (uint8_t*)malloc(cart->chr.size)))
	{
		fprintf(stderr, "Error: unable to allocate CHR memory (code %d)\n", errno);
		cartridge_unload(cart);
		fclose(rom);
		return -1;
	}
	if (!(cart->prg_ram.data = (uint8_t*)malloc(cart->prg_ram.size)))
	{
		fprintf(stderr, "Error: unable to allocate memory for PRG RAM (code %d)\n", errno);
		cartridge_unload(cart);
		fclose(rom);
		return -1;
	}

	if (fseek(rom, rom_start_ofs, SEEK_CUR) != 0 || fread(cart->prg_rom.data, 1, cart->prg_rom.size, rom) != cart->prg_rom.size)
	{
		fprintf(stderr, "Error: unable to read PRG ROM (code %d)\n", errno);
		cartridge_unload(cart);
		fclose(rom);
		return -1;
	}
	if (header[5] && fread(cart->chr.data, 1, cart->chr.size, rom) != cart->chr.size)
	{
		fprintf(stderr, "Error: unable to read CHR ROM (code %d)\n", errno);
		cartridge_unload(cart);
		fclose(rom);
		return -1;
	}
	fclose(rom);
	if (mapper_init(&cart->mapper, (struct Cartridge*)cart, mapper_num) != 0)
	{
		fprintf(stderr, "Error: unable to initialize mapper\n");
		cartridge_unload(cart);
		return -1;
	}
	return 0;
}

void cartridge_unload(Cartridge* cart)
{
	free(cart->prg_rom.data);
	free(cart->prg_ram.data);
	free(cart->chr.data);
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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "game.h"
#include "mapper.h"

static void game_parse_header(Game* g, FILE* rom)
{
	/* iNES ROM header format
	   Byte     Contents
	   ---------------------------------------------------------------------------
	   0-3      String "NES^Z" used to recognize .NES files.
	   4        Number of 16kB ROM banks.
	   5        Number of 8kB VROM banks.
	   6        bit 0     1 for vertical mirroring, 0 for horizontal mirroring.
				bit 1     1 for battery-backed RAM at $6000-$7FFF.
				bit 2     1 for a 512-byte trainer at $7000-$71FF.
				bit 3     1 for a four-screen VRAM layout. 
				bit 4-7   Four lower bits of ROM Mapper Type.
	   7        bit 0     1 for VS-System cartridges.
				bit 1-3   Reserved, must be zeroes!
				bit 4-7   Four higher bits of ROM Mapper Type.
	   8        Number of 8kB RAM banks. For compatibility with the previous
		        versions of the .NES format, assume 1x8kB RAM page when this
			    byte is zero.
	   9        bit 0     1 for PAL cartridges, otherwise assume NTSC.
		        bit 1-7   Reserved, must be zeroes!
	   10-15    Reserved, must be zeroes!
	   16-...   ROM banks, in ascending order. If a trainer is present, its
	            512 bytes precede the ROM bank contents.
	   ...-EOF  VROM banks, in ascending order.
	   --------------------------------------------------------------------------- */

	char magic[4], reserved[6];
	uint8_t flags1, flags2, flags3, mapper_num;

	/* TODO: error checking (are reserved portions of the header null?) */
	fread(magic, 1, 4, rom);

	/* if (memcmp(magic, "NES\x1A", 4) != 0) {
		ERROR
	} */

	fread(&g->prg_rom_banks, 1, 1, rom);
	fread(&g->chr_banks, 1, 1, rom);
	fread(&flags1, 1, 1, rom);
	fread(&flags2, 1, 1, rom);
	fread(&g->prg_ram_banks, 1, 1, rom);
	fread(&flags3, 1, 1, rom);
	fread(reserved, 1, 6, rom);

	/* Following 6 bits are reserved and should be 0 */

	if (flags1 & 0x01)
		g->mirror_mode = MIRRORING_VERTICAL;
	else if (flags1 & 0x04)
		g->mirror_mode = MIRRORING_4SCREEN;
	else
		g->mirror_mode = MIRRORING_HORIZONTAL;

	if (flags3 & 0x01)
		g->video_mode = VIDEO_PAL;
	else
		g->video_mode = VIDEO_NTSC;

	g->has_nvram = (flags1 & 0x02);
	g->has_trainer = (flags1 & 0x04);
	mapper_num = (flags1 & 0xF0) >> 4;

	g->is_vs_game = (flags2 & 0x01);
	/* if (flags2 & 0x0E) {
		ERROR (reserved bits)
	} */
	mapper_num |= (flags2 & 0xF0);

	/* TODO: look into "DiskDude!" headers (messes up mapper number detection) */
	g->mapper = mapper_get(mapper_num);
	if (!g->mapper)
	{
		printf("Unsupported mapper (%d)\n", mapper_num);
		exit(1);
	}
}

void game_load(Game* g, char* path)
{
	uint32_t prg_size, chr_size;
	FILE* rom = fopen(path, "rb");		/* TODO: check fopen succeeded */
	memset(g, 0, sizeof(*g));
	game_parse_header(g, rom);

	prg_size = 0x4000 * g->prg_rom_banks * sizeof(uint8_t);
	chr_size = 0x2000 * g->chr_banks * sizeof(uint8_t);

	/* TODO: check malloc() succeeds */

	/* Get and load data bank(s) */
	g->prg_mem = (uint8_t*)malloc(prg_size);
	fread(g->prg_mem, 1, prg_size, rom);

	/* Using CHR RAM */
	if (chr_size == 0)
		g->chr_mem = (uint8_t*)malloc(0x2000);
	else
	{
		g->chr_mem = (uint8_t*)malloc(chr_size);
		fread(g->chr_mem, 1, chr_size, rom);
	}

	fclose(rom);
	g->mapper->init(g);

	/* TODO: return success/failure */
}
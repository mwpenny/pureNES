#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>

#include "mapper.h"

typedef enum {
	MIRRORING_VERTICAL,
	MIRRORING_HORIZONTAL,
	MIRRORING_4SCREEN
} MirrorMode;

typedef enum {
	VIDEO_NTSC,
	VIDEO_PAL
} VideoMode;

typedef struct Cartridge
{
	MirrorMode mirror_mode;
	VideoMode video_mode;
	Mapper mapper;
	uint8_t has_nvram;
	uint8_t *prg_rom, *prg_ram, *chr;
	uint32_t prg_rom_size, prg_ram_size, chr_size;
} Cartridge;

void cartridge_load(Cartridge* cart, char* path);
void cartridge_unload(Cartridge* cart);

uint8_t cartridge_read(Cartridge* cart, uint16_t addr);
void cartridge_write(Cartridge* cart, uint16_t addr, uint8_t val);

#endif

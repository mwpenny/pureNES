#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>

#include "mappers/mapper.h"

#define ADDRESSABLE_PRG_ROM 0x8000
#define ADDRESSABLE_PRG_RAM 0x2000
#define ADDRESSABLE_CHR 0x2000

typedef enum {
	MIRRORING_VERTICAL,
	MIRRORING_HORIZONTAL,
	MIRRORING_4SCREEN
} MirrorMode;

typedef enum {
	VIDEO_NTSC,
	VIDEO_PAL
} VideoMode;

typedef struct {
	uint8_t* data;
	uint32_t size;
} Memory;

typedef struct Cartridge
{
	MirrorMode mirror_mode;
	VideoMode video_mode;
	Mapper mapper;
	uint8_t has_nvram;
	Memory prg_rom, prg_ram, chr;
} Cartridge;

int cartridge_load(Cartridge* cart, char* path);
void cartridge_unload(Cartridge* cart);

uint8_t cartridge_read(Cartridge* cart, uint16_t addr);
void cartridge_write(Cartridge* cart, uint16_t addr, uint8_t val);

#endif

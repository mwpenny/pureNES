#ifndef GAME_H
#define GAME_H

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

struct Mapper;
typedef struct
{
	uint8_t prg_rom_banks;		/* bank count (size = 16KB) */
	uint8_t prg_ram_banks;		/* bank count (size = 8KB; 0 infers 8KB for compatibility) */
	uint8_t chr_banks;			/* bank count (size = 8KB; 0 indicates CHR RAM) */
	uint8_t has_nvram;			/* i.e., battery-backed PRG RAM */
	uint8_t has_trainer;
	uint8_t is_vs_game;
	MirrorMode mirror_mode;
	VideoMode video_mode;
	Mapper* mapper;

	uint8_t *prg_mem, *chr_mem;
	uint8_t *prg_bank1, *prg_bank2;
	uint8_t *chr_bank1, *chr_bank2;
} Game;

void game_load(Game* g, char* path);

#endif
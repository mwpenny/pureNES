#include <stdint.h>

#include "../game.h"
#include "../mapper.h"

static void init(Game* game)
{
	game->prg_bank1 = game->prg_mem;
	/* NROM-128: mirror first PRG bank into second */
	if (game->prg_rom_banks == 1)
		game->prg_bank2 = game->prg_mem;
	else
		game->prg_bank2 = game->prg_mem + 0x4000;

	game->chr_bank1 = game->chr_mem;
	game->chr_bank2 = game->chr_mem + 0x1000;
}

static void write(Game* game, uint16_t addr, uint8_t val) {};

Mapper mapper_nrom = {init, write};
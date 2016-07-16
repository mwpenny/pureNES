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

	game->chr_bank = game->chr_mem;
}

Mapper mapper_nrom = {init};
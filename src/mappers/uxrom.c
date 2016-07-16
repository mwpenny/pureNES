#include <stdint.h>

#include "../game.h"
#include "../mapper.h"

static void init(Game* game)
{
	game->prg_bank1 = game->prg_mem;
	game->prg_bank2 = game->prg_mem + 0x4000*(game->prg_rom_banks-1);
	game->chr_bank1 = game->chr_mem;
	game->chr_bank2 = game->chr_mem + 0x1000;
}

static void write(Game* game, uint16_t addr, uint8_t val)
{
	/* TODO: when does the bank switching actually occur on real hardware? */
	if (addr > 0x7FFF && addr < 0x10000)
	{
		/* TODO: UNROM uses bits 2-0; UOROM uses bits 3-0 */
		game->prg_bank1 = game->prg_mem + (val & 0xF)*0x4000;
	}
}

Mapper mapper_uxrom = {init, write};
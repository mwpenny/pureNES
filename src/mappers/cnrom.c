/* CNROM board (and similar; iNES mapper 3):
	-16KB or 32KB PRG ROM (no bankswitching)
	-1 8KB CHR bank, switchable
*/

#include "../cartridge.h"
#include "mapper.h"

/* TODO: bus conflict related bugs in Cybernoid and Colorful Dragon */

static void reset(Mapper* mapper)
{
	mapper_set_prg_rom_bank(mapper, 0, 0);
	mapper_set_prg_ram_bank(mapper, 0, 0);
	mapper_set_chr_bank(mapper, 0, 0);
	if (mapper->cartridge->prg_rom.size > 0x4000)
		mapper_set_prg_rom_bank(mapper, 1, -1);
	else
		mapper_set_prg_rom_bank(mapper, 1, 0);
}

static void write(Mapper* mapper, uint16_t addr, uint8_t val)
{
	/* Select 8 KB CHR ROM bank for PPU $0000-$1FFF */
	mapper_set_chr_bank(mapper, 0, val);
}

int cnrom_init(Mapper* mapper)
{
	mapper->prg_rom_banks.bank_count = 1;
	mapper->prg_ram_banks.bank_count = 1;
	mapper->chr_banks.bank_count = 1;
	mapper->reset = reset;
	mapper->write = write;
	return 0;
}

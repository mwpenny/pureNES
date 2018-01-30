/* NROM (NES-NROM-128 and NES-NROM-256 boards; iNES mapper 0):
	-16KB or 32KB PRG ROM (no bankswitching)
*/

#include "../cartridge.h"
#include "mapper.h"

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
	/* No bank switching or special logic */
}

int nrom_init(Mapper* mapper)
{
	mapper->prg_rom_banks.bank_count = 2;
	mapper->prg_ram_banks.bank_count = 1;
	mapper->chr_banks.bank_count = 1;
	mapper->reset = reset;
	mapper->write = write;
	return 0;
}

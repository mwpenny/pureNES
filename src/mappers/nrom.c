/* NROM mapper (iNES mapper 0):
	-16KB or 32KB PRG ROM (no bankswitching)
*/

#include "../cartridge.h"
#include "../mapper.h"

static void reset(Mapper* mapper)
{
	mapper->prg_rom_banks.banks[0] = mapper->cartridge->prg_rom.data;
	mapper->prg_ram_banks.banks[0] = mapper->cartridge->prg_ram.data;
	mapper->chr_banks.banks[0] = mapper->cartridge->chr.data;

	/* There is only one bank, so our {addressable_memory / bank_count} formula
	   doesn't work here. The bank size is just the size of PRG ROM */
	mapper->prg_rom_banks.bank_size = mapper->cartridge->prg_rom.size;
}

static void write(Mapper* mapper, uint16_t addr, uint8_t val)
{
	/* No bank switching or special logic */
}

int nrom_init(Mapper* mapper)
{
	mapper->prg_rom_banks.bank_count = 1;
	mapper->prg_ram_banks.bank_count = 1;
	mapper->chr_banks.bank_count = 1;
	mapper->reset = reset;
	mapper->write = write;
	return 0;
}

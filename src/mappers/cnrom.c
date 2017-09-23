/* CNROM board (and similar; iNES mapper 3):
	-16KB or 32KB PRG ROM (no bankswitching)
	-1 8KB CHR bank, switchable
*/

#include "../cartridge.h"
#include "../mapper.h"

/* TODO: bus conflict related bugs in Cybernoid and Colorful Dragon */

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
	/* Select CHR bank */
	uint32_t ofs = (val * 0x2000) % mapper->cartridge->chr.size;
	mapper->chr_banks.banks[0] = mapper->cartridge->chr.data + ofs;
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

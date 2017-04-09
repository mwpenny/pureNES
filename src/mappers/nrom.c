/* NROM mapper (iNES mapper 0):
	-16KB or 32KB PRG ROM (no bankswitching)
*/

#include "../cartridge.h"
#include "../mapper.h"

static void reset(Mapper* mapper)
{
	mapper->prg_rom_banks.banks[0] = mapper->cartridge->prg_rom;
	mapper->prg_ram_banks.banks[0] = mapper->cartridge->prg_ram;
	mapper->chr_banks.banks[0] = mapper->cartridge->chr;
	mapper->prg_rom_banks.bank_size = mapper->cartridge->prg_rom_size;
}

static void write(Mapper* mapper, uint16_t addr, uint8_t val) {}

MapperConfig mapper_conf_nrom = {1, 1, 1, reset, write};

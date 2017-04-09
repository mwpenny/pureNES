/* UxROM mapper (iNES mapper 2):
	-2 16KB PRG ROM banks
	  -First is switchable
	  -Second is fixed to the last 16KB of PRG ROM
*/

#include "../cartridge.h"
#include "../mapper.h"

static void reset(Mapper* mapper)
{
	mapper->prg_rom_banks.banks[0] = mapper->cartridge->prg_rom;
	mapper->prg_rom_banks.banks[1] = mapper->cartridge->prg_rom + mapper->cartridge->prg_rom_size - 0x4000;
	mapper->prg_ram_banks.banks[0] = mapper->cartridge->prg_ram;
	mapper->chr_banks.banks[0] = mapper->cartridge->chr;
}

static void write(Mapper* mapper, uint16_t addr, uint8_t val)
{
	/* Select region of ROM for bank 0 */
	uint32_t ofs = ((val & 0xF) * 0x4000) % mapper->cartridge->prg_rom_size;
	mapper->prg_rom_banks.banks[0] = mapper->cartridge->prg_rom + ofs;
}

MapperConfig mapper_conf_uxrom = {2, 1, 1, reset, write};

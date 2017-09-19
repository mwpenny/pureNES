/* UxROM mapper (iNES mapper 2):
	-2 16KB PRG ROM banks
	  -First is switchable
	  -Second is fixed to the last 16KB of PRG ROM
*/

#include "../cartridge.h"
#include "../mapper.h"

static void reset(Mapper* mapper)
{
	mapper->prg_rom_banks.banks[0] = mapper->cartridge->prg_rom.data;

	/* Bank 1 is fixed to the last bank */
	mapper->prg_rom_banks.banks[1] = mapper->cartridge->prg_rom.data + \
									 mapper->cartridge->prg_rom.size - \
									 mapper->prg_rom_banks.bank_size;
	mapper->prg_ram_banks.banks[0] = mapper->cartridge->prg_ram.data;
	mapper->chr_banks.banks[0] = mapper->cartridge->chr.data;
}

static void write(Mapper* mapper, uint16_t addr, uint8_t val)
{
	/* Select region of ROM for bank 0 */
	uint32_t ofs = ((val & 0xF) * 0x4000) % mapper->cartridge->prg_rom.size;
	mapper->prg_rom_banks.banks[0] = mapper->cartridge->prg_rom.data + ofs;
}

int uxrom_init(Mapper* mapper)
{
	mapper->prg_rom_banks.bank_count = 2;
	mapper->prg_ram_banks.bank_count = 1;
	mapper->chr_banks.bank_count = 1;
	mapper->reset = reset;
	mapper->write = write;
	return 0;
}

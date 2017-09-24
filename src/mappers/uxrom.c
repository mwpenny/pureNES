/* UxROM (NES-UNROM, NES-UOROM, HVC-UN1ROM their HVC counterparts, and
   clone boards; iNES mapper 2):
	-2 16KB PRG ROM banks
	  -First is switchable
	  -Second is fixed to the last 16KB of PRG ROM
*/

#include "../cartridge.h"
#include "mapper.h"

static void reset(Mapper* mapper)
{
	mapper_set_prg_rom_bank(mapper, 0, 0);
	mapper_set_prg_rom_bank(mapper, 1, -1);
	mapper_set_prg_ram_bank(mapper, 0, 0);
	mapper_set_chr_bank(mapper, 0, 0);
}

static void write(Mapper* mapper, uint16_t addr, uint8_t val)
{
	/* 7  bit  0
	   ---- ----
	   xxxx pPPP
		    ||||
		    ++++- Select 16KB PRG ROM bank for CPU $8000-$BFFF
			      (UNROM uses bits 2-0; UOROM uses bits 3-0) */
	mapper_set_prg_rom_bank(mapper, 0, val & 0xF);
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

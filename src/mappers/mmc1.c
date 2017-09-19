/* MMC1 mapper (iNES mapper 1):
	-2 16KB PRG ROM banks
	  -First is switchable or switched to the first bank
	  -Second is switchable or fixed to the last bank
	-2 4KB CHR banks, both switchable
*/

/* TODO: When the CPU writes to the serial port on consecutive cycles, the MMC1 ignores all writes but the first. */

#include <stdio.h>
#include <stdlib.h>

#include "../cartridge.h"
#include "../mapper.h"

typedef struct {
	uint8_t shift_reg;
	uint8_t** swappable_prg_rom_bank;
} MMC1Data;

static void fix_prg_rom_bank0(Mapper* mapper)
{
	/* Bank1 swappable, bank0 fixed to first bank */
	mapper->prg_rom_banks.banks[0] = mapper->cartridge->prg_rom.data;
	((MMC1Data*)mapper->data)->swappable_prg_rom_bank = &mapper->prg_rom_banks.banks[1];
}

static void fix_prg_rom_bank1(Mapper* mapper)
{
	/* Bank0 swappable, bank1 fixed to last bank */
	uint32_t ofs = mapper->cartridge->prg_rom.size - mapper->prg_rom_banks.bank_size;
	mapper->prg_rom_banks.banks[1] = mapper->cartridge->prg_rom.data + ofs;
	((MMC1Data*)mapper->data)->swappable_prg_rom_bank = &mapper->prg_rom_banks.banks[0];
}

/* Control register
   4bit0
   -----
   CPPMM
   |||||
   |||++- Mirroring (0: one-screen, lower bank; 1: one-screen, upper bank;
   |||               2: vertical; 3: horizontal)
   |++--- PRG ROM bank mode (0, 1: switch 32 KB at $8000, ignoring low bit of bank number;
   |                         2: fix first bank at $8000 and switch 16 KB bank at $C000;
   |                         3: fix last bank at $C000 and switch 16 KB bank at $8000)
   +----- CHR ROM bank mode (0: switch 8 KB at a time; 1: switch two separate 4 KB banks) */
static void write_reg_ctrl(Mapper* mapper, uint8_t val)
{
	/* Mirroring mode */
	switch (val & 3)
	{
		case 0:
		case 1:
			/* TODO: one-screen mirroring */
			break;
		case 2:
			mapper->cartridge->mirror_mode = MIRRORING_VERTICAL;
			break;
		case 3:
			mapper->cartridge->mirror_mode = MIRRORING_HORIZONTAL;
			break;
	}

	/* PRG ROM bank switching mode */
	switch ((val >> 2) & 3)
	{
		case 0:
		case 1:
			mapper->prg_rom_banks.bank_size = 0x8000;
			break;
		case 2:
			mapper->prg_rom_banks.bank_size = 0x4000;
			fix_prg_rom_bank0(mapper);
			break;
		case 3:
			mapper->prg_rom_banks.bank_size = 0x4000;
			fix_prg_rom_bank1(mapper);
			break;
	}

	/* CHR bank switching mode */
	mapper->chr_banks.bank_size = (val & 0x10) ? 0x1000 : 0x2000;
}

static void write_reg_chr_bank0(Mapper* mapper, uint8_t val)
{
	/* Select 4KB or 8KB CHR bank at PPU $0000. In 8KB mode, even banks
	   are used for bank0 and the following bank is used for bank1
	   (in effect, one large bank) */
	uint32_t ofs;
	if (mapper->chr_banks.bank_size == 0x2000)
		val &= 0x1E;
	else
		val &= 0x1F;
	ofs = (val * mapper->chr_banks.bank_size) % mapper->cartridge->chr.size;
	mapper->chr_banks.banks[0] = mapper->cartridge->chr.data + ofs;

	/* Bank1 will not actually be read from in 8K mode since bank0's
	   size was set to 8K, however, we want bank1 to follow bank0 in
	   case CHR banking is switched back to 4K mode */
	if (mapper->chr_banks.bank_size == 0x2000)
	{
		ofs = (ofs + 0x2000) % mapper->cartridge->chr.size;
		mapper->chr_banks.banks[1] = mapper->cartridge->chr.data + ofs;
	}
}

static void write_reg_chr_bank1(Mapper* mapper, uint8_t val)
{
	/* Select 4KB CHR bank at PPU $1000 (ignored in 8KB mode) */
	if (mapper->chr_banks.bank_size == 0x1000)
	{
		uint32_t ofs = (val * 0x1000) % mapper->cartridge->chr.size;
		mapper->chr_banks.banks[1] = mapper->cartridge->chr.data + ofs;
	}
}

static void write_reg_prg_bank(Mapper* mapper, uint8_t val)
{
	/* Select 16KB or 32KB PRG ROM bank (low bit ignored in 32KB mode) */
	uint32_t ofs;
	if (mapper->prg_rom_banks.bank_size == 0x8000)
		val &= 0xE;
	else
		val &= 0xF;
	ofs = (val * mapper->prg_rom_banks.bank_size) % mapper->cartridge->prg_rom.size;

	/* Both banks act as 1 in 32K mode */
	if (mapper->prg_rom_banks.bank_size == 0x8000)
	{
		mapper->prg_rom_banks.banks[0] = mapper->cartridge->prg_rom.data + ofs;
		ofs = (ofs + 0x4000) % mapper->cartridge->prg_rom.size;
		mapper->prg_rom_banks.banks[1] = mapper->cartridge->prg_rom.data + ofs;
	}
	else
	{
		*((MMC1Data*)mapper->data)->swappable_prg_rom_bank = mapper->cartridge->prg_rom.data + ofs;
	}	

	/* TODO: 5th bit enables/disables PRG RAM (ignored on MMC1A) */
}

static void reset(Mapper* mapper)
{
	((MMC1Data*)mapper->data)->shift_reg = 0x80;
	fix_prg_rom_bank1(mapper);
	mapper->prg_ram_banks.banks[0] = mapper->cartridge->prg_ram.data;
	mapper->chr_banks.banks[0] = mapper->cartridge->chr.data;
	mapper->chr_banks.banks[1] = mapper->cartridge->chr.data + \
								 mapper->cartridge->chr.size - \
								 mapper->chr_banks.bank_size;
}

static void write(Mapper* mapper, uint16_t addr, uint8_t val)
{
	/* The MMC1 is configured via a serial port. Writing a value with bit 7
	   set clears an internal shift register. Otherwise, bit 0 is shifted in.
	   Register contents are written to the control register specified by bits
	   13 and 14 of the address on the fifth write */
	MMC1Data* data = (MMC1Data*)mapper->data;

	/* Reset. The 1 bit is used to determine when the last write occurs */
	if (val & 0x80)
	{
		data->shift_reg = 0x80;
		/*mapper->prg_rom_banks.bank_size = 0x4000;
		fix_prg_rom_bank1(mapper);*/
	}
	else
	{
		/* TODO: When the CPU writes to the serial port on consecutive cycles,
		   the MMC1 ignores all writes but the first */
		data->shift_reg = (data->shift_reg >> 1) | ((val & 1) << 7);

		/* Shifted last bit (5th write) */
		if (data->shift_reg & 4)
		{
			/* Address determines which register to write to */
			data->shift_reg >>= 3;
			switch ((addr >> 13) & 3)
			{
				case 0:
					write_reg_ctrl(mapper, data->shift_reg);
					break;
				case 1:
					write_reg_chr_bank0(mapper, data->shift_reg);
					break;
				case 2:
					write_reg_chr_bank1(mapper, data->shift_reg);
					break;
				case 3:
					write_reg_prg_bank(mapper, data->shift_reg);
					break;
			}
			data->shift_reg = 0x80;
		}
	}
}

int mmc1_init(Mapper* mapper)
{
	if (!(mapper->data = malloc(sizeof(MMC1Data))))
	{
		fprintf(stderr, "Error: unable to allocate memory for MMC1 registers (code %d)\n", errno);
		return -1;
	}
	mapper->prg_rom_banks.bank_count = 2;
	mapper->prg_ram_banks.bank_count = 1;
	mapper->chr_banks.bank_count = 2;
	mapper->reset = reset;
	mapper->write = write;
	return 0;
}

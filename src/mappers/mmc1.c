/* MMC1 mapper (iNES mapper 1):
	-2 16KB PRG ROM banks
	  -First is switchable or switched to the first bank
	  -Second is switchable or fixed to the last bank
	-2 4KB CHR banks, both switchable
*/

/* TODO: When the CPU writes to the serial port on consecutive cycles, the MMC1 ignores all writes but the first. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "../cartridge.h"
#include "mapper.h"

typedef enum {
	BANK_MODE_SINGLE,
	BANK_MODE_DOUBLE
} BankMode;

typedef struct {
	uint8_t shift_reg;
	uint8_t swappable_prg_rom_bank;
	BankMode prg_bank_mode, chr_bank_mode;
} MMC1Data;

static void fix_prg_rom_bank0(Mapper* mapper)
{
	/* Bank1 swappable, bank0 fixed to first bank */
	mapper_set_prg_rom_bank(mapper, 0, 0);
	((MMC1Data*)mapper->data)->swappable_prg_rom_bank = 1;
}

static void fix_prg_rom_bank1(Mapper* mapper)
{
	/* Bank0 swappable, bank1 fixed to last bank */
	mapper_set_prg_rom_bank(mapper, 1, -1);
	((MMC1Data*)mapper->data)->swappable_prg_rom_bank = 0;
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
	MMC1Data* data = ((MMC1Data*)mapper->data);

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
			data->prg_bank_mode = BANK_MODE_SINGLE;
			break;
		case 2:
			data->prg_bank_mode = BANK_MODE_DOUBLE;
			fix_prg_rom_bank0(mapper);
			break;
		case 3:
			data->prg_bank_mode = BANK_MODE_DOUBLE;
			fix_prg_rom_bank1(mapper);
			break;
	}

	if (val & 0x10)
		data->chr_bank_mode = BANK_MODE_DOUBLE;
	else
		data->chr_bank_mode = BANK_MODE_SINGLE;
}

static void write_reg_chr_bank0(Mapper* mapper, uint8_t val)
{
	/* Select 4KB or 8KB CHR bank at PPU $0000. In 8KB mode, even banks
	   are used for bank0 and the following bank is used for bank1
	   (in effect, one large bank) */
	if (((MMC1Data*)mapper->data)->chr_bank_mode == BANK_MODE_SINGLE)
	{
		val = (val & 0x1E) * 2;
		mapper_set_chr_bank(mapper, 0, val);
		mapper_set_chr_bank(mapper, 1, val + 1);
	}
	else
	{
		mapper_set_chr_bank(mapper, 0, val & 0x1F);
	}
}

static void write_reg_chr_bank1(Mapper* mapper, uint8_t val)
{
	/* Select 4KB CHR bank at PPU $1000 (ignored in 8KB mode) */
	if (((MMC1Data*)mapper->data)->chr_bank_mode == BANK_MODE_DOUBLE)
		mapper_set_chr_bank(mapper, 1, val & 0x1F);
}

static void write_reg_prg_bank(Mapper* mapper, uint8_t val)
{
	/* Select 16KB or 32KB PRG ROM bank (low bit ignored in 32KB mode) */
	MMC1Data* data = ((MMC1Data*)mapper->data);
	if (data->prg_bank_mode == BANK_MODE_SINGLE)
	{
		val = (val & 0xE) * 2;
		mapper_set_prg_rom_bank(mapper, 0, val);
		mapper_set_prg_rom_bank(mapper, 1, val + 1);
	}
	else
	{
		mapper_set_prg_rom_bank(mapper, data->swappable_prg_rom_bank, val & 0xF);
	}

	/* TODO: 5th bit enables/disables PRG RAM (ignored on MMC1A) */
}

static void reset(Mapper* mapper)
{
	MMC1Data* data = ((MMC1Data*)mapper->data);
	data->shift_reg = 0x80;
	data->prg_bank_mode = BANK_MODE_DOUBLE;
	data->chr_bank_mode = BANK_MODE_SINGLE;

	mapper_set_prg_rom_bank(mapper, 0, 0);
	fix_prg_rom_bank1(mapper);
	mapper_set_prg_ram_bank(mapper, 0, 0);
	mapper_set_chr_bank(mapper, 0, 0);
	mapper_set_chr_bank(mapper, 1, 1);

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

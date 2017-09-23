/* Custom cartridge hardware */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cartridge.h"
#include "mapper.h"

typedef int (*MapperInitializer)(Mapper* mapper);

/* Implemented mappers */
int nrom_init(Mapper* mapper);
int mmc1_init(Mapper* mapper);
int uxrom_init(Mapper* mapper);
int cnrom_init(Mapper* mapper);

static MapperInitializer initializers[256] =
{
	nrom_init, mmc1_init, uxrom_init, cnrom_init, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static int alloc_banks(MemoryBanks* banks, uint16_t addressable_memory)
{
	banks->banks = (uint8_t**)malloc(banks->bank_count * sizeof(uint8_t*));
	if (!banks->banks)
	{
		return -1;
	}
	banks->bank_size = addressable_memory / banks->bank_count;
	return 0;
}

int mapper_init(Mapper* mapper, struct Cartridge* cart, uint8_t mapper_num)
{
	MapperInitializer init = initializers[mapper_num];
	if (!init)
	{
		fprintf(stderr, "Error: unsupported mapper (%d)\n", mapper_num);
		return -1;
	}
	memset(mapper, 0, sizeof(Mapper));
	mapper->cartridge = cart;

	if (init(mapper) != 0)
	{
		fprintf(stderr, "Error: could not initialize mapper (code %d)\n", errno);
		mapper_cleanup(mapper);
		return -1;
	}
	if (alloc_banks(&mapper->prg_rom_banks, ADDRESSABLE_PRG_ROM) != 0)
	{
		fprintf(stderr, "Error: unable to allocate memory for PRG ROM bank offsets (code %d)\n", errno);
		mapper_cleanup(mapper);
		return -1;
	}
	if (alloc_banks(&mapper->prg_ram_banks, ADDRESSABLE_PRG_RAM) != 0)
	{
		fprintf(stderr, "Error: unable to allocate memory for PRG RAM bank offsets (code %d)\n", errno);
		mapper_cleanup(mapper);
		return -1;
	}
	if (alloc_banks(&mapper->chr_banks, ADDRESSABLE_CHR) != 0)
	{
		fprintf(stderr, "Error: unable to allocate memory for CHR bank offsets (code %d)\n", errno);
		mapper_cleanup(mapper);
		return -1;
	}
	mapper->prg_rom_banks.bank_size = ADDRESSABLE_PRG_ROM / mapper->prg_rom_banks.bank_count;
	mapper->prg_ram_banks.bank_size = ADDRESSABLE_PRG_RAM / mapper->prg_ram_banks.bank_count;
	mapper->chr_banks.bank_size = ADDRESSABLE_CHR / mapper->chr_banks.bank_count;
	mapper->reset(mapper);
	return 0;
}

void mapper_cleanup(Mapper* mapper)
{
	free(mapper->prg_rom_banks.banks);
	free(mapper->prg_ram_banks.banks);
	free(mapper->chr_banks.banks);
	free(mapper->data);
}

uint8_t* mapper_get_banked_mem(MemoryBanks* banks, uint16_t addr)
{
	uint8_t bank_num = (addr / banks->bank_size) % banks->bank_count;
	return &banks->banks[bank_num][addr % banks->bank_size];
}

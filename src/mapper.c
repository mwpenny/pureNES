/* Custom cartridge hardware (i.e., for bank switching) */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cartridge.h"
#include "mapper.h"

/* Implemented mappers */
extern MapperConfig mapper_conf_nrom;
extern MapperConfig mapper_conf_uxrom;

static MapperConfig* mapper_configs[256] = 
{
	&mapper_conf_nrom, NULL, &mapper_conf_uxrom, NULL, NULL, NULL, NULL, NULL,
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

void mapper_init(Mapper* mapper, struct Cartridge* cart, uint8_t mapper_num)
{
	MapperConfig* conf = mapper_configs[mapper_num];

	if (!conf)
	{
		fprintf(stderr, "Unsupported mapper (%d)\n", mapper_num);
		exit(EXIT_FAILURE);
	}
	if (!(mapper->prg_rom_banks.banks = (uint8_t**)malloc(conf->prg_rom_bank_count * sizeof(uint8_t*))))
	{
		fprintf(stderr, "Error: unable to allocate memory for PRG ROM bank offsets (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (!(mapper->prg_ram_banks.banks = (uint8_t**)malloc(conf->prg_ram_bank_count * sizeof(uint8_t*))))
	{
		fprintf(stderr, "Error: unable to allocate memory for PRG RAM bank offsets (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (!(mapper->chr_banks.banks = (uint8_t**)malloc(conf->chr_bank_count * sizeof(uint8_t*))))
	{
		fprintf(stderr, "Error: unable to allocate memory for CHR bank offsets (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	mapper->prg_rom_banks.bank_count = conf->prg_rom_bank_count;
	mapper->prg_rom_banks.bank_size = 0x8000/conf->prg_rom_bank_count;

	mapper->prg_ram_banks.bank_count = conf->prg_ram_bank_count;
	mapper->prg_ram_banks.bank_size = 0x2000/conf->prg_ram_bank_count;

	mapper->chr_banks.bank_count = conf->chr_bank_count;
	mapper->chr_banks.bank_size = 0x2000/conf->chr_bank_count;

	mapper->cartridge = cart;
	mapper->data = NULL;
	mapper->reset = conf->reset;
	mapper->write = conf->write;

	conf->reset(mapper);
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

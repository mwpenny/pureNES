#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>

struct Cartridge;

typedef struct {
	uint8_t** banks;
	uint8_t bank_count;
	uint16_t bank_size;
} MemoryBanks;

typedef void (*MapperResetFunc)(struct Mapper* mapper);
typedef void (*MapperWriteFunc)(struct Mapper* mapper, uint16_t addr, uint8_t val);

typedef struct Mapper {
	struct Cartridge* cartridge;
	void* data;  /* Mapper-specific internal data */
	MapperResetFunc reset;
	MapperWriteFunc write;
	MemoryBanks prg_rom_banks, prg_ram_banks, chr_banks;
} Mapper;

typedef struct {
	uint8_t prg_rom_bank_count, prg_ram_bank_count, chr_bank_count;
	MapperResetFunc reset;
	MapperWriteFunc write;
} MapperConfig;

void mapper_init(Mapper* mapper, struct Cartridge* cart, uint8_t mapper_num);
void mapper_cleanup(Mapper* mapper);

uint8_t* mapper_get_banked_mem(MemoryBanks* banks, uint16_t addr);
void mapper_write(Mapper* mapper, uint16_t addr, uint8_t val);

#endif

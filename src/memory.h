#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>
#include <stdint.h>

typedef struct
{
	uint8_t* ram;		/* RAM */
	uint8_t* ppu_reg;	/* PPU registers */
	uint8_t* prg1;		/* lower PRG-ROM bank */
	uint8_t* prg2;		/* upper PRG-ROM bank */
} Memory;

void memory_map(uint8_t** ptr, uint8_t* dest);
uint8_t* memory_get_mapped_address(Memory* mem, uint16_t addr);

uint8_t memory_get(Memory* mem, uint16_t addr);
uint16_t memory_get16(Memory* mem, uint16_t addr);
uint16_t memory_get16_ind(Memory* mem, uint16_t addr);
void memory_set(Memory* mem, uint16_t addr, uint8_t val);

#endif
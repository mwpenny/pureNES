#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "ppu.h"

typedef struct
{
	uint8_t* ram;
	PPU* ppu;
	uint8_t* prg1;
	uint8_t* prg2;
} Memory;


/* TODO: Table of function pointers to handle memory mapping.
		 Either map functions to types of memory (e.g., ppu registers, PRG-ROM)
		 or have a lookup table for every byte of addressable memory (wasteful, but extensible) */

uint8_t memory_get(Memory* mem, uint16_t addr);
uint16_t memory_get16(Memory* mem, uint16_t addr);
uint16_t memory_get16_ind(Memory* mem, uint16_t addr);
void memory_set(Memory* mem, uint16_t addr, uint8_t val);

#endif
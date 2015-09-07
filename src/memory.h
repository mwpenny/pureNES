#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "nes.h"

/* TODO: Table of function pointers to handle memory mapping.
		 Either map functions to types of memory (e.g., ppu registers, PRG-ROM)
		 or have a lookup table for every byte of addressable memory (wasteful, but extensible) */

uint8_t memory_get(NES* nes, uint16_t addr);
uint16_t memory_get16(NES* nes, uint16_t addr);
uint16_t memory_get16_ind(NES* nes, uint16_t addr);
void memory_set(NES* nes, uint16_t addr, uint8_t val);

#endif
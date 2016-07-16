#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "nes.h"

uint8_t memory_get(NES* nes, uint16_t addr);
uint16_t memory_get16(NES* nes, uint16_t addr);
uint16_t memory_get16_ind(NES* nes, uint16_t addr);
void memory_set(NES* nes, uint16_t addr, uint8_t val);

#endif
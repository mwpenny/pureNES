#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

static uint8_t memory[2048];

uint8_t* memory_map(uint16_t addr);

uint8_t memory_get(uint16_t addr);
uint16_t memory_get16(uint16_t addr);
void memory_set(uint16_t addr, uint8_t val);

#endif
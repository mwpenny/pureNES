#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

static uint8_t ram[2048];
static uint8_t* rom;
static uint32_t rom_size;

uint8_t* memory_get_mapped(uint16_t addr);
void memory_map_rom(uint8_t* rom_addr, uint32_t size);

uint8_t memory_get(uint16_t addr);
uint16_t memory_get16(uint16_t addr);
void memory_set(uint16_t addr, uint8_t val);

#endif
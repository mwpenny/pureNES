#ifndef NES_H
#define NES_H

#include <stdint.h>

#include <SDL.h>

#include "apu.h"
#include "cartridge.h"
#include "controller.h"
#include "cpu.h"
#include "mapper.h"
#include "ppu.h"

#define RAMSIZE 0x800

typedef struct NES
{
	CPU cpu;
	PPU ppu;
	APU apu;
	Controller c1;
	Controller c2;
	uint8_t ram[RAMSIZE];	
	Cartridge cartridge;
} NES;

void nes_init(NES* nes);
int nes_load_rom(NES* nes, char* path);
void nes_update(NES* nes, SDL_Surface* screen);

#endif
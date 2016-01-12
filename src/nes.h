#ifndef NES_H
#define NES_H

#include <stdint.h>

#include "cpu.h"
#include "ppu.h"
#include <SDL.h>

#define RAMSIZE 0x800

typedef struct NES
{
	CPU cpu;
	PPU ppu;
	uint8_t ram[RAMSIZE];
	uint8_t* prg1;
	uint8_t* prg2;
	uint8_t* vrom;
} NES;

void nes_init(NES* nes);
void nes_load_rom(NES* nes, char* path);
void nes_update(NES* nes, SDL_Surface* screen);

#endif
#ifndef NES_H
#define NES_H

#include <stdint.h>

#include "cpu.h"
#include "ppu.h"
#include "controller.h"
#include "game.h"

#include <SDL.h>

#define RAMSIZE 0x800

typedef struct NES
{
	CPU cpu;
	PPU ppu;
	Controller c1;
	Controller c2;
	uint8_t ram[RAMSIZE];	
	Game game;
} NES;

void nes_init(NES* nes);
void nes_load_rom(NES* nes, char* path);
void nes_update(NES* nes, SDL_Surface* screen);

#endif
#ifndef NES_H
#define NES_H

#include <stdint.h>

#include "apu.h"
#include "cartridge.h"
#include "controller.h"
#include "cpu.h"
#include "ppu.h"

#define RAMSIZE 0x800

typedef void (*RenderCallback)(uint32_t* frame, void* userdata);

typedef struct NESInitInfo {
	RenderCallback render_callback;
	void* render_userdata;
} NESInitInfo;

typedef struct NES {
	CPU cpu;
	PPU ppu;
	APU apu;
	Controller c1;
	Controller c2;
	uint8_t ram[RAMSIZE];	
	Cartridge cartridge;
} NES;

void nes_init(NES* nes, NESInitInfo* init_info);
int nes_load_rom(NES* nes, char* path);
int nes_update(NES* nes);

#endif
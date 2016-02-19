#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include "renderer.h"

typedef struct {
	struct NES* nes;
	
	uint8_t io_latch;
	uint8_t x;	   /* Fine x scroll (3 bits) */
	uint8_t w;	   /* First/second write flag */

	/* Current and temp VRAM address (15 bits each)
	   yyy NN YYYYY XXXXX
	   ||| || ||||| +++++-- coarse X scroll
	   ||| || +++++-------- coarse Y scroll
	   ||| ++-------------- nametable select
	   +++----------------- fine Y scroll */
	uint16_t v, t;

	/* PPU control register
		7  bit  0
		---- ----
		VPHB SINN
		|||| ||||
		|||| ||++- Base nametable address
		|||| ||    (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00)
		|||| |+--- VRAM address increment per CPU read/write of PPUDATA
		|||| |     (0: add 1, going across; 1: add 32, going down)
		|||| +---- Sprite pattern table address for 8x8 sprites
		||||       (0: $0000; 1: $1000; ignored in 8x16 mode)
		|||+------ Background pattern table address (0: $0000; 1: $1000)
		||+------- Sprite size (0: 8x8; 1: 8x16)
		|+-------- PPU master/slave select
		|          (0: read backdrop from EXT pins; 1: output color on EXT pins)
		+--------- Generate an NMI at the start of the
				   vertical blanking interval (0: off; 1: on)*/
	uint8_t ppuctrl;

	/* PPU mask register
		7  bit  0
		---- ----
		BGRs bMmG
		|||| ||||
		|||| |||+- Grayscale (0: normal color, 1: produce a greyscale display)
		|||| ||+-- 1: Show background in leftmost 8 pixels of screen, 0: Hide
		|||| |+--- 1: Show sprites in leftmost 8 pixels of screen, 0: Hide
		|||| +---- 1: Show background
		|||+------ 1: Show sprites
		||+------- Emphasize red*
		|+-------- Emphasize green*
		+--------- Emphasize blue*
		* NTSC colors. PAL and Dendy swaps green and red */
	uint8_t ppumask;

	/* Flags for PPUSTATUS ($2002) */
	uint8_t spr_overflow;
	uint8_t szero_hit;
	uint8_t vblank_started;

	uint8_t oam_addr;
	uint8_t data_buf;

	/* PPU memory */
	uint8_t vram[2048];
	uint8_t oam[256];
	uint8_t pram[32];

	uint16_t scanline;
	uint16_t cycle;

	/* TODO: make everything below internal */
	uint8_t bg_attr1, bg_attr2;
	uint16_t bg_bmp1, bg_bmp2;

	uint8_t soam[32]; /* Secondary OAM */
	uint8_t spr_attr[8];
	uint8_t spr_x[8];
	uint8_t spr_bmp1[8], spr_bmp2[8];

} PPU;

void ppu_init(PPU* ppu, struct NES* nes);
void ppu_write(PPU* ppu, uint16_t addr, uint8_t val);
uint8_t ppu_read(PPU* ppu, uint16_t addr);

void ppu_step(PPU* ppu, RenderSurface screen);

#endif
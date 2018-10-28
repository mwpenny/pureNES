#ifndef PPU_H
#define PPU_H

#include <stdint.h>

typedef struct {
		uint8_t palette;
		uint8_t back_priority;
		uint8_t flip_x;
		uint8_t flip_y;
		uint8_t x;
		uint8_t bmp_lo, bmp_hi;
		uint8_t idx;
} Sprite;

typedef void (*RenderCallback)(uint32_t* frame, void* userdata);

struct NES;
struct NESInitInfo;

typedef struct {
	struct NES* nes;
	RenderCallback render_cb;
	void* render_userdata;
	uint32_t framebuffer[256*240];
	
	/* Current and temp VRAM address (15 bits each)
	   yyy NN YYYYY XXXXX
	   ||| || ||||| +++++-- coarse X scroll
	   ||| || +++++-------- coarse Y scroll
	   ||| ++-------------- nametable select
	   +++----------------- fine Y scroll */
	uint16_t v, t;

	uint8_t x;  /* Fine x scroll */
	uint8_t w;  /* First/second write flag */
	uint8_t odd_frame;
	uint8_t spr_overflow;
	uint8_t spr0_hit;
	uint8_t vblank_started;

	uint8_t io_latch;  /* Last value written to a PPU register */
	uint8_t ppuctrl;
	uint8_t ppumask;
	uint8_t oamaddr;
	uint8_t data_buf;  /* For buffered reads from PPUDATA ($2007) */

	/* Object attribute (sprite) memory. 64 entries. 4 bytes/sprite
		Byte 0: sprite Y position
		Byte 1: sprite tile
			For 8x8 sprites: tile number
			For 8x16 sprites:
			76543210
			||||||||
			|||||||+- Bank ($0000 or $1000) of tiles
			+++++++-- Tile number of top of sprite (0 to 254; bottom half is next tile)
		Byte 2: sprite attributes
		76543210
		||||||||
		||||||++- Palette (4 to 7) of sprite
		|||+++--- Unimplemented
		||+------ Priority (0: in front of background; 1: behind background)
		|+------- Flip sprite horizontally
		+-------- Flip sprite vertically
		Byte 3: sprite x position */
	uint8_t oam[256];
	uint8_t soam[32];    /* Secondary OAM */
	uint8_t spr_indices[8];
	uint8_t vram[2048];  /* i.e., nametables */
	uint8_t pram[32];    /* Palette memory */

	/* Used during sprite evaluation */
	uint8_t oam_buf;  /* For buffered OAM reads */
	uint8_t spr_in_range;
	uint8_t overflow_checked;
	uint8_t soam_idx;
	uint8_t oam_searched;

	/* Internal latches and shift registers for use during rendering */
	uint8_t bg_tile_idx;
	uint8_t bg_attr_latch;
	uint16_t bg_bmp_latch;
	uint16_t bg_attr_lo, bg_attr_hi;
	uint16_t bg_bmp_lo, bg_bmp_hi;
	Sprite scanline_sprites[8];

	uint16_t scanline, cycle;
} PPU;

/*void ppu_oamdata_write(PPU* ppu, uint8_t val);*/
void ppu_init(PPU* ppu, struct NES* nes, struct NESInitInfo* init_info);
void ppu_write(PPU* ppu, uint16_t addr, uint8_t val);
uint8_t ppu_read(PPU* ppu, uint16_t addr);
void ppu_tick(PPU* ppu);

#endif

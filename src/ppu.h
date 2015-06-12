#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <SDL.h>

/* PPU VRAM map (from http://nesdev.com/NESDoc.pdf)

$0000 to $1FFF: pattern tables
  $0000 to $0FFF: pattern table 0
  $1000 to $1FFF: pattern table 1
$2000 to 3EFF: nametables
  $2000 to $23FF: nametable 0
  $2400 to $27FF: nametable 1
  $2800 to $2BFF: nametable 2
  $2C00 to $2FFF: nametable 3
  $3000 to $3EFF: mirrors $2000 to $2EFF
$3F00 to $3FFF: palettes (13 colors per palette)
  $3F00 to $3F0F: image palette
    $3F00: background color (every 4 bytes mirror this)
  $3F10 to $3F1F: sprite palette
  $3F20 to $3FFF: mirrors $3F00 to $3F1F
$4000 to $10000: mirrors $0000 to $3FFF

*/

/* PPU OAM map (from http://nesdev.com/NESDoc.pdf)

byte 0: sprite y coordinate - 1 (from top)
byte 1: sprite tile number (index in pattern tables)
byte 2: sprite attribute
  bits 0 to 1: most significant 2 bits of the color
  bit 5		 : whether sprite has priority over background (0=high, 1=low)
  bit 6		 : whether to flip sprite horizontally
  bit 7		 : whether to flip sprite vertically
byte 3: sprite x coordinate

*/

/* TODO: Actually decode the video signal */
static uint32_t palette[64] =
{
	0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000, 0x881400,
	0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
	0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC, 0xE40058, 0xF83800, 0xE45C10,
	0xAC7C00, 0x00B800, 0x00A800, 0x00A844, 0x008888, 0x000000, 0x000000, 0x000000,
	0xF8F8F8, 0x3CBCFC, 0x6888FC, 0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044,
	0xF8B800, 0xB8F818, 0x58D854, 0x58F898, 0x00E8D8, 0x787878, 0x000000, 0x000000,
	0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8,
	0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8, 0x000000, 0x000000
};

typedef struct
{
	uint8_t ctrl;		/* write */
	uint8_t mask;		/* write */
	uint8_t status;		/* read */

	uint8_t oam_addr;	/* write */
	uint8_t oam_data;	/* read / write */

	uint8_t scroll;		/* write */
	uint8_t vram_addr;	/* read */
	uint8_t vram_data;	/* read / write*/

	uint8_t oam_dma;	/* read / write */

	/*uint8_t vram[16384];*/
	uint8_t* vram;
	uint8_t oam[256]; /* SPR-RAM */
} PPU;

void ppu_render_pattern_table(PPU* ppu, SDL_Surface* screen);

#endif
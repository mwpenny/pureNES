#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <SDL.h>

struct CPU;
struct Memory;

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

#define PPU_BA_MASK 0x0C00
#define PPU_COARSEX_MASK 0x001F
#define PPU_FINEX_MASK 0x0007

#define PPU_COARSEY_MASK 0x03E0
#define PPU_FINEY_MASK 0x7000

#define PPU_VRAM_ADDRH_MASK 0x3F00

typedef struct
{
	/* TODO: remove unused registers. Some mapped memory just uses functions */
	uint8_t io_latch;
	uint8_t addr_latch;

	uint8_t ctrl;		/* write */
	uint8_t mask;		/* write */
	uint8_t status;		/* read */

	uint8_t oam_addr;	/* write */

	/*
	v and t bits:

	yyy NN YYYYY XXXXX
	||| || ||||| +++++-- coarse X scroll
	||| || +++++-------- coarse Y scroll
	||| ++-------------- nametable select
	+++----------------- fine Y scroll
	*/

	uint16_t v, t; /* VRAM address and temp address (top left tile). 15 bits */
	uint8_t x; /* fine x scroll. 3 bits */

	/*uint8_t vram[16384];*/
	uint8_t* vram;
	uint8_t oam[256]; /* SPR-RAM */

	struct CPU* cpu; /* TODO: only needed for sending NMI. Maybe just store function pointer
			           to NMI-generating function. Then no need to know about CPU */
} PPU;

void ppu_init(PPU* ppu, struct CPU* cpu);

/* PPU register read/write handlers */
uint8_t ppu_read_ctrl(PPU* ppu);
void ppu_write_ctrl(PPU* ppu, uint8_t val);

uint8_t ppu_read_mask(PPU* ppu);
void ppu_write_mask(PPU* ppu, uint8_t val);

uint8_t ppu_read_status(PPU* ppu);
void ppu_write_status(PPU* ppu, uint8_t val);

uint8_t ppu_read_oam_addr(PPU* ppu);
void ppu_write_oam_addr(PPU* ppu, uint8_t val);

uint8_t ppu_read_oam_data(PPU* ppu);
void ppu_write_oam_data(PPU* ppu, uint8_t val);

uint8_t ppu_read_scroll(PPU* ppu);
void ppu_write_scroll(PPU* ppu, uint8_t val);

uint8_t ppu_read_vram_addr(PPU* ppu);
void ppu_write_vram_addr(PPU* ppu, uint8_t val);

uint8_t ppu_read_vram_data(PPU* ppu);
void ppu_write_vram_data(PPU* ppu, uint8_t val);

uint8_t ppu_read_oam_dma(PPU* ppu);
void ppu_write_oam_dma(PPU* ppu, uint8_t val);

/* TODO: dynamically set these?? */
/* Lookup tables */
static uint8_t (*ppu_read_handlers[8])(PPU* ppu) = 
{
	ppu_read_ctrl,
	ppu_read_mask,
	ppu_read_status,
	ppu_read_oam_addr,
	ppu_read_oam_data,
	ppu_read_scroll,
	ppu_read_vram_addr,
	ppu_read_vram_data
};

static void (*ppu_write_handlers[8])(PPU* ppu, uint8_t val) = 
{
	ppu_write_ctrl,
	ppu_write_mask,
	ppu_write_status,
	ppu_write_oam_addr,
	ppu_write_oam_data,
	ppu_write_scroll,
	ppu_write_vram_addr,
	ppu_write_vram_data
};

uint8_t ppu_read(PPU* ppu, uint16_t addr);
void ppu_write(PPU* ppu, uint16_t addr, uint8_t val);

void ppu_render_pattern_table(PPU* ppu, SDL_Surface* screen);
void ppu_render_nametable(PPU* ppu, SDL_Surface* screen);

#endif
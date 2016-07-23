#include <stdint.h>

#include "nes.h"
#include "memory.h"
#include "renderer.h"
#include "ppu.h"
#include "game.h"

/* TODO: XOR vs AND/OR */

/*** PPUCTRL ($2000) macros ***/
#define NAMETABLE(ppu) ((ppu->ppuctrl & 3) * 0x400) + 0x2000 /* TODO: NECESSARY? */
#define VADDR_INC(ppu) (((ppu->ppuctrl >> 2) & 1) * 31) + 1
#define SPR_TBL(ppu) ((ppu->ppuctrl >> 3) & 1) * 0x1000
#define BG_TBL(ppu) ((ppu->ppuctrl >> 4) & 1) * 0x1000
#define SPR_SIZE(ppu) ((ppu->ppuctrl >> 5) & 1)
#define MSTR_SLV(ppu) ((ppu->ppuctrl >> 6) & 1)
#define NMI_ENABLED(ppu) ((ppu->ppuctrl >> 7) & 1)

/*** PPUMASK ($2001) macros ***/
#define GRAYSCALE(ppu) (ppu->ppumask & 1)
#define BG_LEFT_ENALED(ppu) ((ppu->ppumask >> 1) & 1)
#define SPR_LEFT_ENABLED(ppu) ((ppu->ppumask >> 2) & 1)
#define BG_ENABLED(ppu) ((ppu->ppumask >> 3) & 1)
#define SPR_ENABLED(ppu) ((ppu->ppumask >> 4) & 1)

/* NTSC colors. PAL and Dendy swap green and red */
#define EMPH_R(ppu) ((ppu->ppumask >> 5) & 1)
#define EMPH_G(ppu) ((ppu->ppumask >> 6) & 1)
#define EMPH_B(ppu) ((ppu->ppumask >> 7) & 1)

/* TODO: Actually decode the video signal? */
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

static uint8_t ppu_mem_read(PPU* ppu, uint16_t addr)
{
	/* Pattern tables ($0000-$1FFF) */
	if (addr < 0x1000)
		return ppu->nes->game.chr_bank1[addr];
	else if (addr < 0x2000)
		return ppu->nes->game.chr_bank2[addr - 0x1000];
	/* Namtables ($2000-$2FFF; $3F00-$3F1F mirror $2000-$2EFF) */
	else if (addr < 0x3F00)
	{
		/* TODO: less branching? */
		/*uint8_t vm = GAME_VERT_MIRRORING((&ppu->nes->game));
		uint8_t row = (addr >> 11) & 1;

		return ppu->vram[(addr & (0x3FF | (vm<<10))) + row*0x400*!vm];*/

		/* Handle nametable mirroring */
		if (ppu->nes->game.mirror_mode == MIRRORING_VERTICAL)
			return ppu->vram[addr & 0x7FF];
		else if (ppu->nes->game.mirror_mode == MIRRORING_HORIZONTAL)
		{
			uint8_t row = (addr >> 11) & 1;
			return ppu->vram[addr & 0x3FF | (row << 10)];
		}
		/*else
		{
			TODO: OTHER MIRRORINGS
		}*/
	}
	/* Palettes ($3F00-$3F1F; mirrored at $3F20-$3FFF) */
	else if (addr < 0x4000)
	{
		/* TODO: look into background palette hack */

		uint8_t a = (addr-0x3F00)%0x20;

		/* Palette background mirroring.
		   $3F04/$3F08/$3F0C can contain unique data, but
		   $3F10/$3F14/$3F18/$3F1C mirror $3F00/$3F04/$3F08/$3F0C */
		/* Necessary? */
		if (a > 15 && (a % 4) == 0)
			a = 0;

		/* Grayscale */
		if (ppu->ppumask & 1)
			return ppu->pram[a] & 0x30;
		return ppu->pram[a];
	}
	/* Out of bounds. Oh shit... */
	return 0;
}

static void ppu_mem_write(PPU* ppu, uint16_t addr, uint8_t val)
{
	/* Pattern tables ($0000-$1FFF) */
	if (addr < 0x2000)
		ppu->nes->game.chr_bank1[addr] = val;
	else if (addr < 0x2000)
		ppu->nes->game.chr_bank2[addr - 0x1000] = val;
	/* Namtables ($2000-$2FFF; $3F00-$3F1F mirror $2000-$2EFF) */
	else if (addr < 0x3F00)
	{
		/* Handle nametable mirroring */
		if (ppu->nes->game.mirror_mode == MIRRORING_VERTICAL)
			ppu->vram[addr & 0x7FF] = val;
		else if (ppu->nes->game.mirror_mode == MIRRORING_HORIZONTAL)
		{
			uint8_t row = (addr >> 11) & 1;
			ppu->vram[addr & 0x3FF | (row << 10)] = val;
		}
		/*else
		{
			TODO: OTHER MIRRORINGS
		}*/
	}
	/* Palettes ($3F00-$3F1F; mirrored at $3F20-$3FFF) */
	else if (addr < 0x4000)
	{
		/* TODO: look into background palette hack */

		uint8_t a = (addr-0x3F00)%0x20;

		/* Palette background mirroring.
		   $3F04/$3F08/$3F0C can contain unique data, but
		   $3F10/$3F14/$3F18/$3F1C mirror $3F00/$3F04/$3F08/$3F0C */
		if (a > 15 && (a % 4) == 0)
			a = 0;
		ppu->pram[a] = val;
	}
	else
	{
		/* Out of bounds. Oh shit... */
	}
}

/* Handles writes to $2000 (PPU control register) */
static void ppuctrl_write(PPU* ppu, uint8_t val)
{
	ppu->ppuctrl = val;

	/* Copy base nametable address
	   t: ...BA.. ........ = d: ......BA */
	ppu->t = (ppu->t & 0x73FF) | ((val & 3) << 10);
}

/* Handles writes to $2001 (PPU mask register) */
static void ppumask_write(PPU* ppu, uint8_t val)
{
	ppu->ppumask = val;
}

/* Handles reads from $2002 (PPU status register) */
static uint8_t ppustatus_read(PPU* ppu)
{
	uint8_t vbl, ret;

	/* Reading PPUSTATUS at the exact start of vblank will
	   return 0 in bit 7 but clear the latch anyway,
	   causing the program to miss frames */
	/* TODO: macro to check vblank? */
	if (ppu->scanline == 241 && ppu->cycle == 1)
		vbl = 0;
	else
		vbl = ppu->vblank_started;

	ret = (vbl << 7) |
		  (ppu->szero_hit << 6) |
		  (ppu->spr_overflow << 5) |
		  (ppu->io_latch & 0x1F);

	ppu->vblank_started = 0;
	ppu->w = 0;

	return ret;
}

/* Handles writes to $2003 (OAM address port) */
static void oamaddr_write(PPU* ppu, uint8_t val)
{
	ppu->oamaddr = val;
}

/* Handles reads from $2004 (OAM data port) */
static uint8_t oamdata_read(PPU* ppu)
{
	/* TODO: under certain conditions, the contents of soam are returned */

	/* Attempting to read during rendering returns $FF (a signal
	   is active that makes the read always return $FF) */
	if (ppu->scanline < 240 && ppu->cycle > 0 && ppu->cycle < 65)
		return 0xFF;
	return ppu->oam[ppu->oamaddr & 0xFF];
}

/* Handles writes to $2004 (OAM data port) */
static void oamdata_write(PPU* ppu, uint8_t val)
{
	/* TODO: Writes to OAMDATA during rendering (on the pre-render
	   line and the visible lines 0-239, provided either sprite or
	   background rendering is enabled) do not modify values in OAM,
	   but do perform a glitchy increment of OAMADDR, bumping only the
	   high 6 bits (i.e., it bumps the [n] value in PPU sprite evaluation
	   - it's plausible that it could bump the low bits instead depending
	   on the current status of sprite evaluation). This extends to DMA transfers
	   via OAMDMA, since that uses writes to $2004. For emulation purposes,
	   it is probably best to completely ignore writes during rendering. */

	/* TODO: In the 2C07, sprite evaluation can never be fully disabled,
	   and will always start 20 scanlines after the start of vblank[9]
	   (same as when the prerender scanline would have been on the 2C02).
	   As such, you must upload anything to OAM that you intend to within
	   the first 20 scanlines after the 2C07 signals vertical blanking. */

	/* Zero out unused bits in attribute byte */
	if ((ppu->oamaddr & 3) == 2)
		val &= 0xE3;
	ppu->oam[ppu->oamaddr++] = val;
}

/* Handles writes to $2005 (PPU scrolling position register) */
static void ppuscroll_write(PPU* ppu, uint8_t val)
{
	if (!ppu->w) /* first write = x offset */
	{
		/* Update coarse and fine X
		   t: ....... ...HGFED = d: HGFED...
		   x:              CBA = d: .....CBA */
		ppu->t = (ppu->t & 0x7FE0) | ((val >> 3) & 0x1F);
		ppu->x = val & 7;		
	}
	else		 /* second write = y offset */
	{
		/* Update coarse and fine Y
		   t: CBA..HG FED..... = d: HGFEDCBA */
		ppu->t = (ppu->t & 0xC1F) | ((val & 7) << 12) |
				 ((val & 0xF8) << 2);
	}

	ppu->w = !ppu->w;
}

/* Handles writes to $2006 (PPU address register) */
static void ppuaddr_write(PPU* ppu, uint8_t val)
{
	if (!ppu->w) /* first write = high byte */
	{
		 /* t: .FEDCBA ........ = d: ..FEDCBA
		    t: X...... ........ = 0 */
		ppu->t = (ppu->t & 0xFF) | ((val & 0x3F) << 8);
	}
	else		 /* second write = low byte */
	{
		/* t: ....... HGFEDCBA = d: HGFEDCBA
		   v                   = t */
		ppu->v = ppu->t = (ppu->t & 0x7F00) | val;
	}

	ppu->w = !ppu->w;
}

/* Handles reads from $2007 (PPU data port) */
static uint8_t ppudata_read(PPU* ppu)
{
	uint8_t val;

	/* Reading with v in [0, $3EFF] returns contents of internal buffer */
	if (ppu->v < 0x3F00)
	{
		val = ppu->data_buf;
		ppu->data_buf = ppu_mem_read(ppu, ppu->v);
	}

	/* If v is in palette range, the buffer is updated with the
	   mirrored nametable data that would appear "underneath" the palette */
	else
	{
		val = ppu_mem_read(ppu, ppu->v);
		ppu->data_buf = ppu_mem_read(ppu, ppu->v - 0x1000);
	}

	/* TODO: During rendering (on the pre-render line and the visible lines 0-239,
	   provided either background or sprite rendering is enabled),
	   it will update v in an odd way...
	   
	   Used by Young Indiana Jones Chronicles and Burai Fighter */

	/* Increment VRAM address */
	ppu->v += VADDR_INC(ppu);

	return val;
}

/* Handles writes to $2007 (PPU data port) */
static void ppudata_write(PPU* ppu, uint8_t val)
{
	ppu_mem_write(ppu, ppu->v, val);

	/* TODO: During rendering (on the pre-render line and the visible lines 0-239,
	   provided either background or sprite rendering is enabled),
	   it will update v in an odd way...
	   
	   Used by Young Indiana Jones Chronicles and Burai Fighter */

	/* Increment VRAM address */
	ppu->v += VADDR_INC(ppu);
}

/* Handles writes to $4014 (OAM DMA register) */
static void oamdma_write(PPU* ppu, uint8_t val)
{
	uint16_t addr = val << 8; /* $XX00 - $XXFF */
	uint16_t i = 0;

	for (; i < 256; ++i)
		oamdata_write(ppu, memory_get(ppu->nes, addr++));

	/* TODO: clean this up */
	ppu->nes->cpu.dma_cycles = 513 + ppu->nes->cpu.oddcycle;
}

void ppu_init(PPU* ppu, NES* nes)
{
	memset(ppu, 0, sizeof(*ppu));
	ppu->nes = nes;

	/*ppu->cycle = 340;
	ppu->scanline = 240;*/
}

/*** PPU access via memory-mapped registers ***/

/* TODO: lookup table? */
void ppu_write(PPU* ppu, uint16_t addr, uint8_t val)
{
	ppu->io_latch = val;

	switch (addr) {
		case 0x2000:
			ppuctrl_write(ppu, val);
			break;
		case 0x2001:
			ppumask_write(ppu, val);
			break;
		case 0x2002:
			break;
		case 0x2003:
			oamaddr_write(ppu, val);
			break;
		case 0x2004:
			oamdata_write(ppu, val);
			break;
		case 0x2005:
			ppuscroll_write(ppu, val);
			break;
		case 0x2006:
			ppuaddr_write(ppu, val);
			break;
		case 0x2007:
			ppudata_write(ppu, val);
			break;
		case 0x4014:
			oamdma_write(ppu, val);
			break;

		/* Invalid address!! */
		default:
			break;		
	}
}

uint8_t ppu_read(PPU* ppu, uint16_t addr)
{
	switch (addr) {
		/* Reading from write-only registers returns open bus */
		case 0x2000:
		case 0x2001:
		case 0x2003:
		case 0x2005:
		case 0x2006:
		case 0x4014:
			return ppu->io_latch;
		case 0x2002:
			return ppustatus_read(ppu);
		case 0x2004:
			return oamdata_read(ppu);
		case 0x2007:
			return ppudata_read(ppu);

		/* Invalid address!! */
		default:
			return 0;	
	}
}

/*** Debug functions ***/
void render_palettes(PPU* ppu, RenderSurface screen)
{
	uint8_t i = 0;
	for (; i < 32; ++i)
	{
		render_pixel(screen, i, 0, palette[ppu->pram[i]]);
	}

	renderer_flip_surface(screen);
}
void render_nt(PPU* ppu, RenderSurface screen)
{
	int i = 0, row, pixel;
	for (; i < 960; ++i)
	{
		uint8_t ntb = ppu_mem_read(ppu, 0x2000 + i);

		for (row = 0; row < 8; ++row)
		{
			for (pixel = 0; pixel < 8; ++pixel)
			{
				uint8_t bit = 7 - pixel;
				uint8_t mask = 1 << bit;				
				uint8_t lb = (ppu_mem_read(ppu, BG_TBL(ppu) + (ntb*16)+row) & mask) >> bit;
				uint8_t hb = (ppu_mem_read(ppu, BG_TBL(ppu) + (ntb*16)+row+8) & mask) >> bit;

				uint8_t color = lb | (hb << 1);
			
				/* Donkey Kong palette 0 (for testing) */
				switch (color)
				{
					case 0:
						color = 0x0F; break;
					case 1:
						color = 0x15; break;
					case 2:
						color = 0x2C; break;
					case 3:
						color = 0x06; break;				
				}

				render_pixel(screen, (i%32)*8 + pixel, (i/32)*8 + row, palette[color]);
			}
		}
	}
}
void render_oam(PPU* ppu, RenderSurface screen)
{
	int i = 0, row, pixel;
	for (; i < 64; ++i)
	{
		for (row = 0; row < 8; ++row)
		{
			for (pixel = 0; pixel < 8; ++pixel)
			{
				uint8_t bit = 7 - pixel;
				uint8_t mask = 1 << bit;				
				uint8_t lb = (ppu_mem_read(ppu, SPR_TBL(ppu) + (ppu->oam[(i*4)+1]*16)+row) & mask) >> bit;
				uint8_t hb = (ppu_mem_read(ppu, SPR_TBL(ppu) + (ppu->oam[(i*4)+1]*16)+row+8) & mask) >> bit;

				uint8_t color = lb | (hb << 1);
			
				/* Donkey Kong palette 0 (for testing) */
				switch (color)
				{
					case 0:
						color = 0x0F; break;
					case 1:
						color = 0x15; break;
					case 2:
						color = 0x2C; break;
					case 3:
						color = 0x06; break;				
				}

				render_pixel(screen, (i%32)*8 + pixel, (i/32)*8 + row, palette[color]);
			}
		}
	}
}

static uint8_t bg_get_tile_palette(PPU* ppu)
{
	/* Fetch attribute byte for current tile */

	/* Attribute table addresses are of the form
	   NN 1111 YYY XXX
	   || |||| ||| +++-- high 3 bits of coarse X (x/4)
	   || |||| +++------ high 3 bits of coarse Y (y/4)
	   || ++++---------- attribute offset (960 bytes)
	   ++--------------- nametable select */
	uint16_t addr = 0x23C0 | (ppu->v & 0xC00) |
					((ppu->v >> 4) & 0x38) |
					((ppu->v >> 2) & 7);

	/* Attribute table bytes control 4x4 tile regions and are of the form
	   BR BL TR TL
	   || || || ++--- Top left 2x2 region's palette index
	   || || ++------ Top right 2x2 region's palette index
	   || ++--------- Bottom left 2x2 region's palette index
	   ++------------ Bottom right 2x2 region's palette index

	   Use scroll values to isolate palette index for current tile's 2x2 tile region

	   Shift right 2 every 4 X tiles (right 2x2 subregions in 4x4 tile regions).
	   Shift right 4 every 2 Y tiles (bottom 2x2 subregions in 4x4 tile regions). */
	return (ppu_mem_read(ppu, addr) >>
			((ppu->v & 2) | ((ppu->v >> 4) & 4))) & 3;
}
static uint8_t bg_get_tile_sliver(PPU* ppu, uint8_t tile, uint8_t hb)
{
	/* Fetch byte of background tile bitmap row from pattern table */
	/* TODO: deduce low/high byte from address? */
	/* TODO: get once. Don't need EXACT hb/lb sliver behavior? (probably not).

	   We're our own people, we can make our own data structures,
	   with blackjack and hookers! */
	uint8_t fineY = (ppu->v >> 12) & 7;
	return ppu_mem_read(ppu, BG_TBL(ppu) + tile*16 + fineY + hb*8);
}

static uint8_t spr_get_tile_sliver(PPU* ppu, uint8_t si, uint8_t hb)
{
	/* Fetch byte of sprite tile bitmap row from pattern table */
	/* TODO: deduce low/high byte from address? */
	/* TODO: get once. Don't need EXACT hb/lb sliver behavior? (probably not).

	   We're our own people, we can make our own data structures,
	   with blackjack and hookers! */

	/* Sprites in secondary OAM have already been verified as being on the
	   current scanline, therefore the difference between their Y coordinates
	   and the current scanline number will not overflow an 8-bit integer */
	uint8_t y = ppu->scanline - ppu->soam[si*4];
	uint8_t attr = ppu->spr_attr[si];
	uint8_t tile = ppu->soam[si*4+1];

	uint16_t addr;
	uint8_t sliver;

	/* 8x16 sprites */
	if (ppu->ppuctrl & 0x20)
	{
		/* Vertical flip */
		if (attr & 0x80)
			y = 15 - y;

		/* Tile bit 0 is used for pattern table select.
		   Bits 1-7 are used for the top tile number. The
		   bottom tile is the next one in the pattern table. */
		addr = (tile&1)*0x1000;
		tile = (tile & 0xFE) + y/8;
		y = y & 0xF7;	/* Y must be within the tile (< 8) */
	}
	/* 8x8 sprites */
	else
	{
		/* Vertical flip */
		if (attr & 0x80)
			y = 7 - y;
		addr = SPR_TBL(ppu);
	}
	sliver = ppu_mem_read(ppu, addr + tile*16 + y + hb*8);

	/* Horizontal flip. Swap single bits, then bit pairs, then finally nibbles */
	if (attr & 0x40)
	{
		sliver = ((sliver & 0xAA) >> 1) | ((sliver & 0x55) << 1);
		sliver = ((sliver & 0xCC) >> 2) | ((sliver & 0x33) << 2);
		return (sliver >> 4) | (sliver << 4);
	}
	return sliver;
}

static void scroll_inc_x(PPU* ppu)
{
	/* Increment coarse X, switching the
	   horizontal nametable on wraparound */
	if ((ppu->v & 0x1F) == 0x1F)
		ppu->v ^= 0x41F;
	else
		++ppu->v;
}

static void scroll_inc_y(PPU* ppu)
{
	/* Switch vertical nametable on wraparound */
	if ((ppu->v & 0x7000) == 0x7000)
	{
		uint8_t coarseY = (ppu->v >> 5) & 0x1F;
		ppu->v &= ~0x7000; /* Clear fine Y */

		/* Last row of nametable. Set coarse Y to 0 and switch NT */
		if (coarseY == 29)
		{
			ppu->v &= 0x7C1F;
			ppu->v ^= 0x0800;
		}

		/* If coarse Y is incremented from 31, it will
			wrap to 0, but the nametable will not switch */
		else if (coarseY == 31)
			ppu->v &= 0x7C1F;

		/* Increment coarse Y */
		else
			ppu->v += 0x20;
	}

	/* Increment fine Y */
	else
		ppu->v += 0x1000;
}

void bg_fetch_tile(PPU* ppu)
{
	static uint8_t nt_byte = 0, attr_byte = 0;
	static uint32_t tile_bmap_low = 0, tile_bmap_hi = 0;

	switch (ppu->cycle % 8)
	{
		case 1:		/* Fetch tile id from current nametable */
			nt_byte = ppu_mem_read(ppu, 0x2000 | (ppu->v & 0xFFF));
			break;
		case 3:		/* Fetch tile attribute */
			attr_byte = bg_get_tile_palette(ppu);
			break;
		case 5:		/* Fetch low byte of tile bitmap row from pattern table */
			tile_bmap_low = bg_get_tile_sliver(ppu, nt_byte, 0);
			break;
		case 7:		/* Fetch high byte of tile bitmap row from pattern table */
			tile_bmap_hi = bg_get_tile_sliver(ppu, nt_byte, 1);
			break;
		case 0:		/* Move data from internal latches to shift registers */
			ppu->bg_attr1 = (ppu->bg_attr1 & 0xFF00) | ((attr_byte & 0x01) * 0xFF);
			ppu->bg_attr2 = (ppu->bg_attr2 & 0xFF00) | (((attr_byte >> 1) & 0x01) * 0xFF);

			ppu->bg_bmp1 = (ppu->bg_bmp1 & 0xFF00) | tile_bmap_low;
			ppu->bg_bmp2 = (ppu->bg_bmp2 & 0xFF00) | tile_bmap_hi;

			/* Move to next tile */
			scroll_inc_x(ppu);
			break;
	}
}

void find_sprites(PPU* ppu)
{
	uint8_t spr_height = ((ppu->ppuctrl & 0x20) >> 5)*8 + 8;

	if (ppu->cycle == 0)
	{
		ppu->soam_idx = 0;
		ppu->spr_in_range = 0;
		ppu->ovr_check_done = 0;
		ppu->oamend = 0;
	}
	else if (!(ppu->cycle & 1))
	{
		uint8_t oam_buf = oamdata_read(ppu);

		/* Clear secondary OAM (reads will have returned $FF) */
		if (ppu->cycle < 65)
		{
			ppu->soam[ppu->soam_idx] = oam_buf;
			ppu->soam_idx = (ppu->soam_idx+1) & 31;
		}
		/* Discover first 8 sprites on the current scanline */
		else if (ppu->cycle < 257)
		{
			uint8_t oamaddr_inc = 0;
			if (!ppu->oamend && !ppu->ovr_check_done)
			{
				ppu->spr_in_range = ppu->spr_in_range || (oam_buf <= ppu->scanline &&
														  oam_buf+spr_height > ppu->scanline);

				/* Fewer than 8 sprites found */
				if (ppu->soam_idx < 32)
					ppu->soam[ppu->soam_idx] = oam_buf;
				else
				{
					/* 2C02 hardware bug! Will result in OAMADDR being
					   incremented by 5 if the sprite is not in range
					   (should only be 4) */
					oamaddr_inc = 1;
					ppu->spr_overflow = ppu->spr_overflow || ppu->spr_in_range;
				}

				if (ppu->spr_in_range)
				{
					/* Reached last byte of this sprite. Stop incrementing soam_idx */
					ppu->spr_in_range = ((ppu->soam_idx++ & 3) != 3);
					oamaddr_inc = 1;
				}
				else
					oamaddr_inc += 4;
			}

			/* Check overflow */
			ppu->oamend = ppu->oamend || (0xFF - ppu->oamaddr < oamaddr_inc);
			ppu->oamaddr = (ppu->oamaddr + oamaddr_inc) & 0xFF;
		}
	}
}

void fetch_next_sprite(PPU* ppu)
{
	/* TODO: If there are fewer than 8 sprites on the next scanline,
			 then dummy fetches to tile $FF occur for the left-over sprites */

	/* TODO: somehow combine with tile fetching logic */
	uint8_t si = (ppu->cycle - 257)/8;
	static uint8_t ccount;

	if (ppu->cycle == 257)
		ccount = 0;

	switch (ccount % 8)
	{
		/* TODO: For the first empty sprite slot, this will consist of sprite #63's Y-coordinate
		   followed by 3 $FF bytes; for subsequent empty sprite slots, this will be four $FF bytes */

		case 2:
			ppu->spr_attr[si] = ppu->soam[(si*4)+2];
			break;
		case 3:
			ppu->spr_x[si] = ppu->soam[(si*4)+3];
			break;
		case 5:		/* Fetch low byte of tile bitmap row from pattern table */
			ppu->spr_bmp1[si] = spr_get_tile_sliver(ppu, si, 0);
			break;
		case 7:		/* Fetch high byte of tile bitmap row from pattern table */
			ppu->spr_bmp2[si] = spr_get_tile_sliver(ppu, si, 1);
			break;
	}

	++ccount;
	ppu->oamaddr = 0;
}

void draw(PPU* ppu, RenderSurface screen)
{
	uint8_t fxo = 15 - ppu->x; /* Fine X offset */
	uint8_t bg_pi = 0;
	uint8_t spr_pi = 0;
	uint8_t si = 0;
	uint32_t px_color = 0;

	uint8_t i = 0;
	for (; i < 8; ++i)
	{
		/*if (ppu->cycle >= ppu->spr_x[i])*/
		if (ppu->spr_x[i] == 0)
		{
			/* TODO: shouldn't this go after? */
			/* TODO: decrementing logic starts at cycle 1 */

			/* First non-transparent pixel moves on to multiplexer
			   (unless using clipping window) */
			if (spr_pi == 0 && (ppu->cycle > 7 || (ppu->ppumask & 0x04)))
			{
				spr_pi = ((ppu->spr_bmp1[i] >> 7) & 1) | ((ppu->spr_bmp2[i] >> 6) & 2);
				si = i;
			}
			ppu->spr_bmp1[i] <<= 1;
			ppu->spr_bmp2[i] <<= 1;
		}
		else
			--ppu->spr_x[i];
	}

	/* Only background color will be shown if using clipping window */
	if (ppu->cycle > 7 || (ppu->ppumask & 0x02))
		bg_pi = ((ppu->bg_bmp1 >> fxo) & 1) | ((ppu->bg_bmp2 >> (fxo-1)) & 2);

	/* BG tile pixel is shown if no overlapping sprite or if overlapping sprite
	   doesn't have priority */
	if (spr_pi == 0 || (bg_pi != 0 && (ppu->spr_attr[si] & 0x20)))
	{
		uint8_t p = ((ppu->bg_attr1 >> fxo) & 1) | ((ppu->bg_attr2 >> (fxo-1)) & 2);
		/* TODO: look into background palette hack */
		/* Palette background mirroring. Although $3F04/$3F08/$3F0C (the
		   background palettes' background color) can contain unique data,
		   only the universal background color is used during rendering. */
		px_color = (bg_pi == 0) ? ppu_mem_read(ppu, 0x3F00) :
			ppu_mem_read(ppu, 0x3F00 | (p*4 + bg_pi));
	}
	else
		px_color = ppu_mem_read(ppu, 0x3F10 | ((ppu->spr_attr[si]&3)*4) + spr_pi);

	/* TODO: check sprite is actually sprite 0 */
	ppu->szero_hit = ppu->szero_hit ||
					 (memcmp(ppu->oam, ppu->soam+si, 4) == 0 &&
					  ((ppu->ppumask & 0x18) == 0x18) &&
					  bg_pi != 0 && spr_pi != 0);
	render_pixel(screen, ppu->cycle, ppu->scanline, palette[px_color]);
}

#define SL_RENDER(sl) (sl < 240)
#define SL_PRERENDER(sl) (sl == 261)
#define CYC_RENDER(cc) (cc > 0 && cc < 257)
#define CYC_PREFETCH(cc) (cc > 320 && cc < 337)

void ppu_step(PPU* ppu, RenderSurface screen)
{
	/* TODO: ***read*** / write on correct cycles */
	/* TODO: init SL to 240 */

	if (BG_ENABLED(ppu) || SPR_ENABLED(ppu))
	{
		/*** Render lines ***/
		if (SL_RENDER(ppu->scanline) || SL_PRERENDER(ppu->scanline))
		{
			/* TODO: "garbage" NT fetches used by MMC5 */
			/* TODO: have this function return a pixel value? */
			if (CYC_RENDER(ppu->cycle) || CYC_PREFETCH(ppu->cycle))
			{
				ppu->bg_attr1 <<= 1;
				ppu->bg_attr2 <<= 1;

				ppu->bg_bmp1 <<= 1;
				ppu->bg_bmp2 <<= 1;

				bg_fetch_tile(ppu);
			}

			/* TODO: this still happens if rendering is disabled!
			   (fixing this will probably cause incorrect sprite overflow behavior
			   (it shouldn't happen when rendering is disabled)) */
			if (SL_RENDER(ppu->scanline) && ppu->cycle < 257)
				find_sprites(ppu);

			/* TODO: have this function return a pixel value? */
			if (ppu->cycle > 256 && ppu->cycle < 321)
				fetch_next_sprite(ppu);

			/* End of line */
			if (ppu->cycle == 256)
				scroll_inc_y(ppu);
			else if (ppu->cycle == 257)
			{
				/* Update v with horizontal data
					v: ....F.. ...EDCBA = t: ....F.. ...EDCBA */
				ppu->v = (ppu->v & 0x7BE0) | (ppu->t & 0x41F);
			}
		}

		if (SL_PRERENDER(ppu->scanline) && ppu->cycle > 279 && ppu->cycle < 305)
		{
			/* Update v with vertical data
			v: IHGF.ED CBA..... = t: IHGF.ED CBA..... */
			ppu->v = (ppu->v & 0x41F) | (ppu->t & 0x7BE0);
		}

		if (SL_RENDER(ppu->scanline) && ppu->cycle > /*0*/-1 && ppu->cycle < 257)
			draw(ppu, screen);
	}

	/* VBLANK start */
	if (ppu->scanline == 241 && ppu->cycle == 1)
	{
		/*render_oam(ppu, screen);*/
		/*render_nt(ppu, screen);*/
		renderer_flip_surface(screen);
		ppu->vblank_started = 1;
		if (NMI_ENABLED(ppu))
			cpu_interrupt(&ppu->nes->cpu, INT_NMI);
	}

	/* Pre-render line (end of VBlank) */
	/* TODO: different length depending on even/odd */
	if (SL_PRERENDER(ppu->scanline) && ppu->cycle == 1)
	{
		ppu->vblank_started = 0;
		ppu->szero_hit = 0;
		ppu->spr_overflow = 0;
	}

	/* Scanline is over */
	if (++ppu->cycle == 342)
	{
		ppu->scanline = (ppu->scanline + 1)%262;
		ppu->cycle = 0;
	}
}
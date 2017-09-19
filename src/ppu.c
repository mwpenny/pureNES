/* Ricoh 2C02 PPU (picture processing unit) emulator.
   Provides NES graphics data manipulation, processing, and output */

#include <stdio.h>

#include "cartridge.h"
#include "memory.h"
#include "nes.h"
#include "ppu.h"
#include "renderer.h"

/* TODO: XOR vs AND/OR */

/* PPUCTRL ($2000) fields */
#define BASE_NAMETABLE (0x2000 + ((ppu->ppuctrl & 3) * 0x400))
#define VRAM_ADDR_INC (1 + (((ppu->ppuctrl >> 2) & 1) * 31))
#define SPR_PATTERN_TABLE (((ppu->ppuctrl >> 3) & 1) * 0x1000)
#define BG_PATTERN_TABLE (((ppu->ppuctrl >> 4) & 1) * 0x1000)
#define TALL_SPRITES (ppu->ppuctrl & 0x20)
#define EXT_OUTPUT (ppu->ppuctrl & 0x40)
#define NMI_ENABLED (ppu->ppuctrl & 0x80)

/* PPUMASK ($2001) fields */
#define GRAYSCALE (ppu->ppumask & 0x01)
#define LEFT_BG_ENABLED (ppu->ppumask & 0x02)
#define LEFT_SPR_ENABLED (ppu->ppumask & 0x04)
#define BG_ENABLED (ppu->ppumask & 0x08)
#define SPR_ENABLED (ppu->ppumask & 0x10)
#define EMPH_R (ppu->ppumask & 0x20)
#define EMPH_G (ppu->ppumask & 0x40)
#define EMPH_B (ppu->ppumask & 0x80)

/* Helper macros for rendering */
#define VISIBLE_LINE (ppu->scanline < 240)
#define PRERENDER_LINE (ppu->scanline == 261)
#define VISIBLE_CYCLE (ppu->cycle >= 1 && ppu->cycle <= 256)
#define PREFETCH_CYCLE (ppu->cycle >= 321 && ppu->cycle <= 336)
#define SPRITE_FETCH_CYCLE (ppu->cycle >= 257 && ppu->cycle <= 320)
#define VBLANK_START (ppu->scanline == 241 && ppu->cycle == 1)

/* NES RGB palette */
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

static uint8_t* dispatch_address(PPU* ppu, uint16_t addr)
{
	if (addr < 0x2000)
		return mapper_get_banked_mem(&ppu->nes->cartridge.mapper.chr_banks, addr);
	else if (addr < 0x3F00)
	{
		/* Nametable mirroring */
		if (ppu->nes->cartridge.mirror_mode == MIRRORING_VERTICAL)
			return &ppu->vram[addr & 0x7FF];
		else if (ppu->nes->cartridge.mirror_mode == MIRRORING_HORIZONTAL)
			return &ppu->vram[(addr & 0x3FF) | ((addr & 0x800) >> 1)];
		else
		{
			/* TODO: 4-screen mirroring and single-screen mirroring */
		}
	}
	else if (addr < 0x4000)
	{
		/* TODO: look into background palette hack */

		/* Palette background mirroring:
		   $3F04/$3F08/$3F0C can contain unique data, but
		   $3F10/$3F14/$3F18/$3F1C mirror $3F00/$3F04/$3F08/$3F0C */
		if ((addr & 0x13) == 0x10)
			return &ppu->pram[addr & 0x0F];
		return &ppu->pram[addr & 0x1F];
	}
	fprintf(stderr, "Warning: invalid PPU memory access ($%04X)\n", addr);
	/*return &ppu->io_latch;*/
	return 0;  /* TODO: return last value on bus */
}

static uint8_t ppu_mem_read(PPU* ppu, uint16_t addr)
{
	return *dispatch_address(ppu, addr);
}

static void ppu_mem_write(PPU* ppu, uint16_t addr, uint8_t val)
{
	/* TODO: disallow writes to CHR-ROM */
	*dispatch_address(ppu, addr) = val;
}

/* Control register ($2000)
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
   +--------- If set, generate an NMI at the start of vblank */
static void ppuctrl_write(PPU* ppu, uint8_t val)
{
	/* Copy base nametable address
	   t: ...BA.. ........ = d: ......BA */
	ppu->t = (ppu->t & 0x73FF) | ((val & 3) << 10);
	ppu->ppuctrl = val;
}

/* Mask register ($2001)
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
   * NTSC colors. PAL and Dendy swap green and red */
static void ppumask_write(PPU* ppu, uint8_t val)
{
	ppu->ppumask = val;
}

/* Status register ($2002)
   7  bit  0
   ---- ----
   VSO. ....
   |||| ||||
   |||+-++++- Least significant bits previously written into a PPU register
   ||+------- Sprite overflow
   |+-------- Sprite 0 hit
   +--------- Vblank started (0: not in vblank; 1: in vblank). */
static uint8_t ppustatus_read(PPU* ppu)
{
	/* Reading PPUSTATUS at the exact start of vblank will return 0 in bit 7 but
	   clear the latch anyway, causing the program to miss frames */
	uint8_t ret = ((!VBLANK_START && ppu->vblank_started) << 7) |
				  (ppu->spr0_hit << 6) |
				  (ppu->spr_overflow << 5) |
				  (ppu->io_latch & 0x1F);
	ppu->vblank_started = 0;
	ppu->w = 0;
	return ret;
}

/* OAM address register ($2003). Address in OAM to access through OAMDATA */
static void oamaddr_write(PPU* ppu, uint8_t val)
{
	ppu->oamaddr = val;
}

/* OAM data port ($2004). Provides read/write to OAM */
static uint8_t oamdata_read(PPU* ppu)
{
	/* TODO: under certain conditions, the contents of soam are returned? */

	/* Attempting to read during rendering returns $FF */
	if (ppu->scanline < 240 && ppu->cycle >= 1 && ppu->cycle <= 64)
		return 0xFF;
	return ppu->oam[ppu->oamaddr];
}
static void oamdata_write(PPU* ppu, uint8_t val)
{
	/* Zero out unused bits in attribute byte */
	if ((ppu->oamaddr & 3) == 2)
		val &= 0xE3;
	ppu->oam[ppu->oamaddr++] = val;
}

/* PPU scroll position register ($2005) */
static void ppuscroll_write(PPU* ppu, uint8_t val)
{
	if ((ppu->w ^= 1))
	{
		/* First write: update coarse and fine X
		   t: ....... ...HGFED = d: HGFED...
		   x:              CBA = d: .....CBA */
		ppu->t = (ppu->t & 0x7FE0) | ((val >> 3) & 0x1F);
		ppu->x = val & 7;		
	}
	else
	{
		/* Second write: update coarse and fine Y
		   t: CBA..HG FED..... = d: HGFEDCBA */
		ppu->t = (ppu->t & 0xC1F) | ((val & 7) << 12) |
				 ((val & 0xF8) << 2);
	}
}

/* PPU address register ($2006). Address is VRAM to access through PPUDATA */
static void ppuaddr_write(PPU* ppu, uint8_t val)
{
	if ((ppu->w ^= 1))
	{
		 /* First write: copy high byte
		    t: .FEDCBA ........ = d: ..FEDCBA
		    t: X...... ........ = 0 */
		ppu->t = (ppu->t & 0xFF) | ((val & 0x3F) << 8);
	}
	else
	{
		/* Second write: copy low byte
		   t: ....... HGFEDCBA = d: HGFEDCBA
		   v                   = t */
		ppu->v = ppu->t = (ppu->t & 0x7F00) | val;
	}
}

/* PPU data port ($2007). Provides read/write to VRAM */
static uint8_t ppudata_read(PPU* ppu)
{
	uint8_t val;
	if (ppu->v < 0x3F00)
	{
		/* Reading when 0 <= v <= $3EFF returns contents of internal buffer */
		val = ppu->data_buf;
		ppu->data_buf = ppu_mem_read(ppu, ppu->v);
	}
	else
	{
		/* If v is in palette range, buffer is updated with mirrored nametable
		   data that would appear "underneath" the palette */
		val = ppu_mem_read(ppu, ppu->v);
		ppu->data_buf = ppu_mem_read(ppu, ppu->v - 0x1000);
	}

	/* X and Y incremented if PPUDATA is accessed during rendering
	   Used by Young Indiana Jones Chronicles and Burai Fighter
	if (ppu->scanline == 261 || ppu->scanline < 240)
	{
		inc_coarse_x(ppu);
		inc_y(ppu);
	}*/
	ppu->v += VRAM_ADDR_INC;
	return val;
}
static void ppudata_write(PPU* ppu, uint8_t val)
{
	/* X and Y incremented if PPUDATA is accessed during rendering
	   Used by Young Indiana Jones Chronicles and Burai Fighter
	if (ppu->scanline == 261 || ppu->scanline < 240)
	{
		inc_coarse_x(ppu);
		inc_y(ppu);
	}*/
	ppu_mem_write(ppu, ppu->v, val);
	ppu->v += VRAM_ADDR_INC;
}

/* OAM DMA register ($4014). Triggers a copy of a block of memory from RAM */
static void oamdma_write(PPU* ppu, uint8_t val)
{
	/* Address range = $XX00 - $XXFF */
	uint16_t addr = val << 8;
	uint16_t i = 0;
	for (; i < 256; ++i)
		oamdata_write(ppu, memory_get(ppu->nes, addr++));
	ppu->nes->cpu.idle_cycles += 513 + (ppu->nes->cpu.cycles & 1);
}

static uint8_t get_bg_palette(PPU* ppu)
{
	/* Fetch the palette attribute for the current tile */

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

	   Use scroll values to isolate palette index for current tile's 2x2 region
	     -Shift right 2 every 2 X tiles (right 2x2 subregions)
	     -Shift right 4 every 2 Y tiles (bottom 2x2 subregions) */
	return (ppu_mem_read(ppu, addr) >>
			((ppu->v & 2) | ((ppu->v >> 4) & 4))) & 3;
}

static uint8_t get_bg_sliver(PPU* ppu, uint8_t tidx, uint8_t hb)
{
	/* Fetch row of background tile bitplane from pattern table.
	   hb = whether or not to fetch the high byte */
	uint8_t y = (ppu->v >> 12) & 7;
	return ppu_mem_read(ppu, BG_PATTERN_TABLE + (tidx * 16) + y + (hb * 8));
}

static uint8_t get_spr_sliver(PPU* ppu, uint8_t sidx, uint8_t hb)
{
	/* Fetch row of sprite tile bitplane from pattern table.
	   hb = whether or not to fetch the high byte */
	Sprite* spr = &ppu->scanline_sprites[sidx];
	uint8_t y = ppu->scanline - ppu->soam[sidx * 4];
	uint8_t tidx = ppu->soam[(sidx * 4) + 1];
	uint16_t addr;
	uint8_t sliver;

	if (TALL_SPRITES)
	{
		if (spr->flip_y)
			y = 15 - y;

		/* 8x16 sprites use tile bit 0 for pattern table select. Bits 1-7 are
		   used for top tile number. Bottom tile is next in the pattern table. */
		addr = (tidx & 1) << 12;
		tidx = (tidx & 0xFE) + ((y & 8) >> 3);
		y &= 7;  /* Keep y relative to tile being rendered (not sprite) */
	}
	else
	{
		if (spr->flip_y)
			y = 7 - y;
		addr = SPR_PATTERN_TABLE;
	}
	sliver = ppu_mem_read(ppu, addr + tidx*16 + y + hb*8);

	if (spr->flip_x)
	{
		sliver = ((sliver & 0xAA) >> 1) | ((sliver & 0x55) << 1);
		sliver = ((sliver & 0xCC) >> 2) | ((sliver & 0x33) << 2);
		return (sliver >> 4) | (sliver << 4);
	}
	return sliver;
}

static void inc_coarse_x(PPU* ppu)
{
	/* Switch horizontal nametable on wraparound */
	if ((ppu->v & 0x1F) == 0x1F)
		ppu->v ^= 0x41F;
	else
		++ppu->v;
}

static void inc_y(PPU* ppu)
{
	/* Switch vertical nametable on wraparound */
	if ((ppu->v & 0x7000) == 0x7000)
	{
		uint8_t coarse_y = (ppu->v >> 5) & 0x1F;
		ppu->v &= ~0x7000;  /* Clear fine Y */

		/* Last row of nametable: switch to next */
		if (coarse_y == 29)
		{
			ppu->v &= 0x7C1F;
			ppu->v ^= 0x0800;
		}

		/* If coarse Y is incremented from 31 ("out of bounds"), it will wrap
		   to 0, but the nametable will not be switched */
		else if (coarse_y == 31)
			ppu->v &= 0x7C1F;
		else
			ppu->v += 0x20;  /* Inc coarse Y */
	}
	else
		ppu->v += 0x1000;  /* Inc fine Y */
}

void fetch_bg_data(PPU* ppu)
{
	switch (ppu->cycle % 8)
	{
		case 1:
			ppu->bg_tile_idx = ppu_mem_read(ppu, 0x2000 | (ppu->v & 0xFFF));
			break;
		case 3:
			ppu->bg_attr_latch = get_bg_palette(ppu);
			break;
		case 5:
			ppu->bg_bmp_latch = get_bg_sliver(ppu, ppu->bg_tile_idx, 0);
			break;
		case 7:
			ppu->bg_bmp_latch |= get_bg_sliver(ppu, ppu->bg_tile_idx, 1) << 8;
			break;
		case 0:
			/* Move data from internal latches to shift registers.
			   Due to timing, the lower 8 bits will already have been shifted out */
			ppu->bg_attr_lo |= (ppu->bg_attr_latch & 0x01) * 0xFF;
			ppu->bg_attr_hi |= ((ppu->bg_attr_latch >> 1) & 0x01) * 0xFF;
			ppu->bg_bmp_lo |= ppu->bg_bmp_latch & 0xFF;
			ppu->bg_bmp_hi |= ppu->bg_bmp_latch >> 8;

			/* Move to next tile */
			inc_coarse_x(ppu);
			break;
	}
}

void find_sprites(PPU* ppu)
{
	/* TODO: better accuracy */
	uint8_t spr_height = ((ppu->ppuctrl & 0x20) >> 5)*8 + 8;

	if (ppu->cycle == 1)
	{
		ppu->soam_idx = 0;
		ppu->spr_in_range = 0;
		ppu->overflow_checked = 0;
		ppu->oam_searched = 0;
	}

	if (ppu->cycle & 1)
	{
		ppu->oam_buf = oamdata_read(ppu);
	}
	else
	{
		/* Clear secondary OAM (reads will have returned $FF) */
		if (ppu->cycle < 65)
		{
			ppu->soam[ppu->soam_idx] = ppu->oam_buf;
			ppu->soam_idx = (ppu->soam_idx + 1) & 31;
		}
		/* Discover first 8 sprites on the current scanline */
		else if (ppu->cycle < 257)
		{
			uint8_t oamaddr_inc = 0;
			if (!ppu->oam_searched && !ppu->overflow_checked)
			{
				ppu->spr_in_range = ppu->spr_in_range || (ppu->oam_buf <= ppu->scanline &&
														  (ppu->oam_buf + spr_height) > ppu->scanline);

				/* Fewer than 8 sprites found */
				if (ppu->soam_idx < 32)
				{
					ppu->soam[ppu->soam_idx] = ppu->oam_buf;
					ppu->spr_indices[ppu->soam_idx/4] = ppu->oamaddr/4;
				}
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
			ppu->oam_searched = ppu->oam_searched || (0xFF - ppu->oamaddr < oamaddr_inc);
			ppu->oamaddr = (ppu->oamaddr + oamaddr_inc) & 0xFF;
		}
	}
}

void fetch_sprite_data(PPU* ppu)
{
	uint8_t sidx = (ppu->cycle - 257) / 8;
	Sprite* spr = &ppu->scanline_sprites[sidx];
	switch (ppu->cycle % 8)
	{
		case 5:
		{
			uint8_t attr = ppu->soam[(sidx * 4) + 2];
			spr->palette = attr & 3;
			spr->back_priority = attr & 0x20;
			spr->flip_x = attr & 0x40;
			spr->flip_y = attr & 0x80;
			spr->x = ppu->soam[(sidx * 4) + 3];
			spr->idx = ppu->spr_indices[sidx];
			spr->bmp_lo = get_spr_sliver(ppu, sidx, 0);
			break;
		}
		case 7:
			spr->bmp_hi = get_spr_sliver(ppu, sidx, 1);
			break;
	}
	ppu->oamaddr = 0;
}

void draw(PPU* ppu, RenderSurface screen)
{
	uint8_t x = ppu->cycle - 1;
	uint8_t bg_pal_idx = 0;
	uint8_t color = 0;

	if (BG_ENABLED && ((x > 7 || LEFT_BG_ENABLED)))
	{
		/* Render background */
		uint8_t shift = 15 - ppu->x;
		uint8_t bg_palette = ((ppu->bg_attr_lo >> shift) & 1) |
							 ((ppu->bg_attr_hi >> (shift - 1)) & 2);
		bg_pal_idx = ((ppu->bg_bmp_lo >> shift) & 1) |
					 ((ppu->bg_bmp_hi >> (shift - 1)) & 2);

		/* TODO: implement background palette hack */
		/* Palette background mirroring. Although $3F04/$3F08/$3F0C can
		   contain unique background data, only the universal background
		   color is used during rendering. */
		color = (bg_pal_idx == 0) ?
				ppu_mem_read(ppu, 0x3F00) :
				ppu_mem_read(ppu, 0x3F00 | (bg_palette*4 + bg_pal_idx));
	}
	if (SPR_ENABLED && (x > 7 || LEFT_SPR_ENABLED))
	{
		/* Render sprites */
		uint8_t i = 0;
		for (; i < 8; ++i)
		{
			Sprite* spr = &ppu->scanline_sprites[i];
			if (x >= spr->x && x < (spr->x + 8))
			{
				uint8_t shift = 7 - (x - spr->x);
				uint8_t pidx = ((spr->bmp_lo >> shift) & 1) |
							   (((spr->bmp_hi >> shift) << 1) & 2);

				/* First non-transparent sprite wins (regardless of priority) */
				if (pidx != 0)
				{
					/* Sprite zero hit: opaque background and opaque sprite on
					   the same pixel */
					/* TODO: doesn't happen at x=255 */
					/* TODO: check sprite is actually sprite 0: OAM may change. memcmp approach isn't reliable */
					ppu->spr0_hit = ppu->spr0_hit || (bg_pal_idx != 0 && spr->idx == 0);
					if (bg_pal_idx == 0 || !spr->back_priority)
						color = ppu_mem_read(ppu, 0x3F10 | ((spr->palette * 4) + pidx));
					break;
				}
			}
		}
	}
	if (GRAYSCALE)
		color &= 0x30;
	render_pixel(screen, x, ppu->scanline, palette[color]);
}

void ppu_tick(PPU* ppu, RenderSurface screen)
{
	/* TODO: ***read*** / write on correct cycles */
	if (BG_ENABLED || SPR_ENABLED)
	{
		if (VISIBLE_LINE)
		{
			if (VISIBLE_CYCLE)
			{
				draw(ppu, screen);
				find_sprites(ppu);
			}
			if (ppu->cycle >= 257 && ppu->cycle <= 320)
				fetch_sprite_data(ppu);
		}
		if (VISIBLE_LINE || PRERENDER_LINE)
		{
			if (VISIBLE_CYCLE || PREFETCH_CYCLE)
			{
				ppu->bg_attr_lo <<= 1;
				ppu->bg_attr_hi <<= 1;
				ppu->bg_bmp_lo <<= 1;
				ppu->bg_bmp_hi <<= 1;
				fetch_bg_data(ppu);
			}
			if (ppu->cycle == 256)  /* End of visible line. Move down a row */
				inc_y(ppu);
			else if (ppu->cycle == 257)
			{
				/* Update v with horizontal data
					v: ....F.. ...EDCBA = t: ....F.. ...EDCBA */
				ppu->v = (ppu->v & 0x7BE0) | (ppu->t & 0x41F);
			}
		}
		if (PRERENDER_LINE && ppu->cycle >= 280 && ppu->cycle <= 304)
		{
			/* Update v with vertical data
			   v: IHGF.ED CBA..... = t: IHGF.ED CBA..... */
			ppu->v = (ppu->v & 0x41F) | (ppu->t & 0x7BE0);
		}
	}
	if (VBLANK_START)
	{
		renderer_flip_surface(screen);
		ppu->vblank_started = 1;

		/* TODO: NMI delay */
		if (NMI_ENABLED)
			cpu_interrupt(&ppu->nes->cpu, INT_NMI);
	}
	else if (PRERENDER_LINE)
	{
		/* TODO: if the sprite address (OAMADDR, $2003) is not zero at the
		   beginning of the pre-render scanline, on the 2C02 an OAM hardware
		   refresh bug will cause the first 8 bytes of OAM to be overwritten
		   by the 8 bytes beginning at OAMADDR & $F8 before sprite evaluation
		   begins */
		if (ppu->cycle == 1)
		{
			/* End of vblank */
			ppu->vblank_started = 0;
			ppu->spr0_hit = 0;
			ppu->spr_overflow = 0;
		}
		else if (ppu->cycle == 339 && ppu->odd_frame && (BG_ENABLED || SPR_ENABLED))
		{
			/* Cycle skipped on odd frames */
			ppu->scanline = 0;
			ppu->cycle = 0;
			ppu->odd_frame = 0;
			return;
		}
	}
	if (++ppu->cycle == 341)
	{
		/* Scanline is over */
		ppu->scanline = (ppu->scanline + 1) % 262;
		if (ppu->scanline == 0)
			ppu->odd_frame ^= 1;
		ppu->cycle = 0;
	}
}

void ppu_init(PPU* ppu, NES* nes)
{
	memset(ppu, 0, sizeof(*ppu));
	ppu->nes = nes;
}

/* PPU access via memory-mapped registers */
void ppu_write(PPU* ppu, uint16_t addr, uint8_t val)
{
	ppu->io_latch = val;
	switch (addr)
	{
		case 0x2000:
			ppuctrl_write(ppu, val);
			break;
		case 0x2001:
			ppumask_write(ppu, val);
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
		default:
			fprintf(stderr, "Warning: invalid PPU write address ($%04X)\n", addr);
			break;
	}
}
uint8_t ppu_read(PPU* ppu, uint16_t addr)
{
	switch (addr)
	{
		case 0x2002:
			return ppustatus_read(ppu);
		case 0x2004:
			return oamdata_read(ppu);
		case 0x2007:
			return ppudata_read(ppu);
		default:
			/* Reading from write-only registers returns
			   last value written to the PPU */
			fprintf(stderr, "Warning: invalid PPU read address ($%04X)\n", addr);
			return ppu->io_latch;
	}
}

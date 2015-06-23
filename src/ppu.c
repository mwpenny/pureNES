#include "ppu.h"

void ppu_init(PPU* ppu)
{
	ppu->io_latch = ppu->addr_latch = 0;
}

uint8_t ppu_read(PPU* ppu, uint16_t addr)
{
	return ppu_read_handlers[(addr - 0x2000) & 7](ppu);
}

void ppu_write(PPU* ppu, uint16_t addr, uint8_t val)
{
	ppu_write_handlers[(addr - 0x2000) & 7](ppu, val);
}

/* TODO: one function to handle all of these? Decay?? */
/* Reading from these registers returns open bus */
uint8_t ppu_read_ctrl(PPU* ppu) { return ppu->io_latch; }
uint8_t ppu_read_mask(PPU* ppu) { return ppu->io_latch; }
uint8_t ppu_read_oam_addr(PPU* ppu) { return ppu->io_latch; }
uint8_t ppu_read_scroll(PPU* ppu) { return ppu->io_latch; }
uint8_t ppu_read_vram_addr(PPU* ppu) { return ppu->io_latch; }
uint8_t ppu_read_oam_dma(PPU* ppu) { return ppu->io_latch; }

void ppu_write_ctrl(PPU* ppu, uint8_t val)
{
	/* TODO: After power/reset, writes to this register are ignored for about 30000 cycles. */

	/*
	Bits 0 and 1:	Base nametable address (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00)
	Bit 2:			VRAM address increment per CPU read/write of PPUDATA (0: add 1, going across; 1: add 32, going down)
	Bit 3:			Sprite pattern table address for 8x8 sprites (0: $0000; 1: $1000; ignored in 8x16 mode)
	Bit 4:			Background pattern table address (0: $0000; 1: $1000)
	Bit 5:			Sprite size (0: 8x8; 1: 8x16)
	Bit 6:			PPU master/slave select (0: read backdrop from EXT pins; 1: output color on EXT pins)
	Bit 7:			Generate an NMI at the start of the vertical blanking interval (0: off; 1: on)
	*/
	/* TODO: check these when rendering */
	/* NOTE: If the PPU is currently in vblank and the vblank flag is set, an NMI will be generated immediately. */
	ppu->io_latch = val;
	ppu->ctrl = val;
}

void ppu_write_mask(PPU* ppu, uint8_t val)
{
	ppu->io_latch = val;
	ppu->mask = val;
}

uint8_t ppu_read_status(PPU* ppu)
{
	/* TODO: fully implement status */
	/* TODO: individual flags or have other functions modify status register? */
	/*uint8_t ret (VBLANK_STARTED << 7) |
		   (SPRITE0_HIT << 6) |
		   (SPR_OVERFLOW << 5) |
		   (ppu->io_latch & 0x1F); /* lower 5 bits of io_latch */
	/*ppu->io_latch = ret;*/
	ppu->addr_latch = 0;
	/* NOTE: Reading PPUSTATUS at exact start of vblank will return 0 in bit 7 but clear the io_latch anyway, 
	         causing the program to miss frames */
	return 0;
	/* CLEAR VBLANK FLAG */
	/* return ret; */
}

void ppu_write_status(PPU* ppu, uint8_t val)
{
	ppu->io_latch = val;
}

void ppu_write_oam_addr(PPU* ppu, uint8_t val)
{
	/* TODO: OAM corruption bug? */
	/* TODO: OAMADDR is set to 0 during each of ticks 257-320 (the sprite tile
	   loading interval) of the pre-render and visible scanlines. */
	ppu->io_latch = val;
	ppu->oam_addr = val;
}

uint8_t ppu_read_oam_data(PPU* ppu)
{
	/* TODO: Reading OAMDATA while the PPU is rendering will expose internal
			 OAM accesses during sprite evaluation and loading; Micro Machines does this */
	ppu->io_latch = ppu->oam[ppu->oam_addr & 0xFF];
	return ppu->io_latch;
}

void ppu_write_oam_data(PPU* ppu, uint8_t val)
{
	/* TODO: Writes to OAMDATA during rendering (on the pre-render line and the
	   visible lines 0-239, provided either sprite or background rendering is
	   enabled) do not modify values in OAM, but do perform a glitchy increment of
	   OAMADDR, bumping only the high 6 bits (i.e., it bumps the [n] value in PPU
	   sprite evaluation - it's plausible that it could bump the low bits instead
	   depending on the current status of sprite evaluation). This extends to DMA
	   transfers via OAMDMA, since that uses writes to $2004. For emulation
	   purposes, it is probably best to completely ignore writes during rendering. */

	ppu->io_latch = val;
	ppu->oam[ppu->oam_addr++ & 0xFF] = val;
}

void ppu_write_scroll(PPU* ppu, uint8_t val)
{
	if (!ppu->addr_latch) /* first write = x offset */
		ppu->scrollx = val;
	else				  /* second write = y offset */
		ppu->scrolly = val;

	ppu->io_latch = val;
	ppu->addr_latch = !ppu->addr_latch;
}

void ppu_write_vram_addr(PPU* ppu, uint8_t val)
{
	if (!ppu->addr_latch) /* first write = high bit */
		ppu->vram_addr = val << 8;
	else				  /* second write = low bit */
		ppu->vram_addr |= val;

	ppu->io_latch = val;
	ppu->addr_latch = !ppu->addr_latch;
}

void inc_vram_addr(PPU* ppu)
{
	if ((ppu->ctrl & 4) >> 2)
		ppu->vram_addr += 32;
	else
		++ppu->vram_addr;
}

uint8_t ppu_read_vram_data(PPU* ppu)
{
	ppu->io_latch = ppu->vram[ppu->vram_addr];
	inc_vram_addr(ppu);
	return ppu->io_latch;
}
void ppu_write_vram_data(PPU* ppu, uint8_t val)
{
	ppu->io_latch = val;
	ppu->vram[ppu->vram_addr] = val;
	inc_vram_addr(ppu);
}

void ppu_write_oam_dma(PPU* ppu, uint8_t val)
{
	/* TODO: this port is actually on the CPU */
	/* TODO: transfer begins at current OAM write address */
	uint8_t i;
	uint16_t addr = val << 8;
	for (i = 0; i < 256; ++i)
	{
	/*	ppu_write_oam_data(ppu, memory_get(mem, addr+i));*/
	}
}

void render_pixel(SDL_Surface* screen, int x, int y, uint32_t color)
{
	/**((uint32_t*)screen->pixels + (x + y*screen->w)) = pixel; */

	uint32_t* pixels = (uint32_t*)screen->pixels;
	pixels[x + (y*screen->w)] = color;
}

void render_tile(PPU* ppu, SDL_Surface* screen, int tile, int x, int y)
{
	int row, pixel;
	uint8_t mask;		/* for isolating the bit corresponding to the pixel */
	uint8_t lb, hb;		/* low and high bytes of the index into the palette */

	uint32_t color;
	uint32_t r = SDL_MapRGB(screen->format, 255, 0, 0);
	uint32_t g = SDL_MapRGB(screen->format, 0, 255, 0);
	uint32_t b = SDL_MapRGB(screen->format, 0, 0, 255);
	
	for (row = 0; row < 8; ++row)
	{
		for (pixel = 0; pixel < 8; ++pixel)
		{
			uint8_t bit = 7 - pixel;
			SDL_Rect box;
			mask = 1 << bit;
			lb = (ppu->vram[(tile*16)+row] & mask) >> bit;
			hb = (ppu->vram[(tile*16)+row+8] & mask) >> bit;

			color = lb | (hb << 1);

			/*if (color == 1) color = r;
			else if (color == 2) color = g;
			else if (color == 3) color = b;*/
			
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

			/* use rect instead of pixels (for scaling) */
			box.x = (x + pixel)*3;
			box.y = (y + row)*3;
			box.w = 3;
			box.h = 3;
			
			/* TODO: implement PPU memory mapping */
			/*color = ppu->vram[0x3F10+curr_palette+color];*/
			SDL_FillRect(screen, &box, palette[color]);
			/*render_pixel(screen, x + pixel, y + row, color);*/
		}
	}
}

void ppu_render_pattern_table(PPU* ppu, SDL_Surface* screen)
{
	int i;

	SDL_LockSurface(screen);

	for (i = 0; i < 256; ++i)
	{
		render_tile(ppu, screen, i, (i%16)*8, (i/16)*8);
	}

	for (i = 256; i < 512; ++i)
	{
		render_tile(ppu, screen, i, 128 + (i%16)*8, ((i-256)/16)*8);
	}

	SDL_UnlockSurface(screen);
	SDL_Flip(screen);
}

void ppu_render_nametable(PPU* ppu, SDL_Surface* screen)
{
	int i;
	SDL_LockSurface(screen);
	for (i = 0; i < 960; ++i) /* nametable 0 */
	{
		int j = ppu->vram[i+0x2000];
		render_tile(ppu, screen, ppu->vram[i + 0x2000], (i%32)*8, (i/32)*8);
	}
	SDL_UnlockSurface(screen);
	SDL_Flip(screen);
}
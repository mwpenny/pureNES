#include "ppu.h"

#include "cpu.h"
void ppu_init(PPU* ppu, CPU* cpu)
{
	ppu->io_latch = ppu->addr_latch = 0;
	ppu->cpu = cpu;
}

uint8_t ppu_read(PPU* ppu, uint16_t addr)
{
	ppu->io_latch = ppu_read_handlers[(addr - 0x2000) & 7](ppu);
	return ppu->io_latch;
}

void ppu_write(PPU* ppu, uint16_t addr, uint8_t val)
{
	ppu->io_latch = val;
	ppu_write_handlers[(addr - 0x2000) & 7](ppu, val);
}

/* TODO: one function to handle all of these? Decay?? */
/* Reading from these registers returns open bus */
uint8_t ppu_read_ctrl(PPU* ppu) { return ppu->io_latch; }
uint8_t ppu_read_mask(PPU* ppu) { return ppu->io_latch; }
void ppu_write_status(PPU* ppu, uint8_t val) {}
uint8_t ppu_read_oam_addr(PPU* ppu) { return ppu->io_latch; }
uint8_t ppu_read_scroll(PPU* ppu) { return ppu->io_latch; }
uint8_t ppu_read_vram_addr(PPU* ppu) { return ppu->io_latch; }
uint8_t ppu_read_oam_dma(PPU* ppu) { return ppu->io_latch; }


/* TODO: "background palette hack" */

#define MASK_CTRL_NTADDR 0x03
#define MASK_CTRL_VINC 0x04
#define MASK_CTRL_SPTBL 0x08
#define MASK_CTRL_BGTBL 0x10
#define MASK_CTRL_SPSIZE 0x20
#define MASK_CTRL_MSTR 0x40 /* TODO: not usually used. No need to emulate? */
#define MASK_CTRL_NMI 0x80

void ppu_write_ctrl(PPU* ppu, uint8_t val)
{
	/* TODO: After power/reset, writes to this register are ignored for about 30000 cycles. */
	/* TODO: check these when rendering */
	/* NOTE: If the PPU is currently in vblank and the vblank flag is set, an NMI will be generated immediately. 
			 Solution: always check bit 7 when in vblank, not just at start */
	ppu->t &= ~PPU_BA_MASK;
	ppu->t |= (val & MASK_CTRL_NTADDR) << 10;
	ppu->ctrl = val; /* TODO: implement the rest like above */
}

#define MASK_MASK_BW 0x01
#define MASK_MASK_BGCROP 0x02
#define MASK_MASK_SPCROP 0x04
#define MASK_MASK_BGD 0x08
#define MASK_MASK_SPR 0x10
#define MASK_MASK_EMPHR 0x20
#define MASK_MASK_EMPHG 0x30
#define MASK_MASK_EMPHB 0x40

void ppu_write_mask(PPU* ppu, uint8_t val)
{
	ppu->mask = val;
}

#define MASK_STATUS_SPZ 0x20
#define MASK_STATUS_OVR 0x30
#define MASK_STATUS_VBL 0x40

uint8_t ppu_read_status(PPU* ppu)
{
	/* NOTE: Reading PPUSTATUS at exact start of vblank will return 0 in bit 7 but clear the io_latch anyway, 
	         causing the program to miss frames */
	/* TODO: vblank flag also cleared at dot 1 of pre-render line */

	uint8_t ret = (ppu->status & 0xE0) | (ppu->io_latch & 0x1F);
	ppu->status &= ~MASK_STATUS_VBL; /* clear vblank flag */
	ppu->addr_latch = 0;
	return ret;
}

void ppu_write_oam_addr(PPU* ppu, uint8_t val)
{
	/* TODO: OAM corruption bug? */
	/* TODO: OAMADDR is set to 0 during each of ticks 257-320 (the sprite tile
	   loading interval) of the pre-render and visible scanlines. */
	ppu->oam_addr = val;
}

uint8_t ppu_read_oam_data(PPU* ppu)
{
	/* TODO: Reading OAMDATA while the PPU is rendering will expose internal
			 OAM accesses during sprite evaluation and loading; Micro Machines does this */
	return ppu->oam[ppu->oam_addr & 0xFF];
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

	ppu->oam[ppu->oam_addr++ & 0xFF] = val;
}

void ppu_write_scroll(PPU* ppu, uint8_t val)
{
	if (!ppu->addr_latch) /* first write = x offset */
	{
		ppu->t &= ~PPU_COARSEX_MASK;
		ppu->t |= val >> 3;

		ppu->x &= ~PPU_FINEX_MASK;
		ppu->x |= val & PPU_FINEX_MASK;
	}
	else				  /* second write = y offset */
	{
		ppu->t &= ~(PPU_COARSEY_MASK | PPU_FINEY_MASK);
		ppu->t |= (val & (PPU_COARSEY_MASK >> 2)) << 2;
		ppu->t |= (val & PPU_FINEX_MASK) << 12;
	}

	ppu->addr_latch = !ppu->addr_latch;
}

void ppu_write_vram_addr(PPU* ppu, uint8_t val)
{
	if (!ppu->addr_latch) /* first write = high bit */
	{
		ppu->t &= ~PPU_VRAM_ADDRH_MASK;
		ppu->t &= 1 << 15;
		ppu->t |= (val & (PPU_VRAM_ADDRH_MASK >> 8)) << 8;
	}
	else				  /* second write = low bit */
	{
		ppu->t &= 0xFF00;
		ppu->t |= val & 0xFF;
		ppu->v = ppu->t;
	}
		

	ppu->addr_latch = !ppu->addr_latch;
}

void inc_vram_addr(PPU* ppu)
{
	if ((ppu->ctrl & 4) >> 2)
		ppu->v += 32;
	else
		++ppu->v;
}

uint8_t ppu_read_vram_data(PPU* ppu)
{
	/* TODO: The PPUDATA read buffer (post-fetch) */
	/* When reading while the VRAM address is in the range 0-$3EFF
	   (i.e., before the palettes), the read will return the contents
	   of an internal read buffer. This internal buffer is updated only
	   when reading PPUDATA, and so is preserved across frames. After the CPU
	   reads and gets the contents of the internal buffer, the PPU will immediately
	   update the internal buffer with the byte at the current VRAM address.
	   Thus, after setting the VRAM address, one should first read this register
	   and discard the result. */
	uint8_t ret = ppu->vram[ppu->v];
	inc_vram_addr(ppu);
	return ret;
}

void ppu_write_vram_data(PPU* ppu, uint8_t val)
{
	ppu->vram[ppu->v] = val;
	inc_vram_addr(ppu);
}

#include "memory.h" /* TODO: rework memory mapping to make this cleaner
							 and remove the circular dependency */
void ppu_write_oam_dma(PPU* ppu, Memory* mem, uint8_t val)
{
	/* TODO: this port is actually on the CPU */
	/* TODO: transfer begins at current OAM write address */
	uint8_t i;
	uint16_t addr = val << 8;
	for (i = 0; i < 256; ++i)
	{
		ppu_write_oam_data(ppu, memory_get(mem, addr+i));
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

void ppu_render_scanline(PPU* ppu, SDL_Surface* screen, uint16_t scanline, uint16_t cycle)
{
	static uint16_t tile1 = 0, tile2 = 0;
	static uint8_t pal1 = 0, pal2 = 0; /* TODO: fed by latch containing palette attribute for next tile */


	

}

void ppu_render(PPU* ppu, SDL_Surface* screen)
{
	static uint16_t scanline = 0; /* 0 to 261 */
	static uint16_t cycle = 0; /* 0 to 340 */

	ppu_render_scanline(ppu, screen, scanline, cycle++);
	if (cycle == 262) ++scanline;

	/* pseudocode */
	/*** NTSC ***/

	/* 262 scanlines per frame
	     -each lasts 341 PPU cycles (113.667 CPU cycles)
		 -1 CPU cycle = 3 PPU cycles

		/*** visible scanlines (0 - 239) ***
			-rendering happens here
			  -1 pixel rendered every clock cycle
			-PPU busy fetching data, shouldn't be accessed by program unless rendering is turned off

			* cycle 0
				-idle cycle
				-value on address bus = CHR address later used to fetch low background tile byte starting at dot 5

			* cycles 1 - 256
				-data for each tile is fetched
				  -each memory access takes 2 PPU cycles, and 4 are performed per tile (8 cycles per tile)
					-nametable byte
					-attribute table byte
					-tile bitmap low (pattern table)
					-tile bitmap high (pattern table)

					-data fetched is placed into internal latches and fed to appropriate shift registers when it's time (every 8 cycles)
					  -PPU can only fetch attribute byte every 8 cycles, so each string of 8 pixels must have the same palette attribute

				-sprite 0 hits act as if the image starts at cycle 2 (same cycles shifters shift for the first time)
				  -sprite 0 flag will be raised at cycle 2 the earliest
					-pixel output delayed further due to internal render pipeline (starts at cycle 4)

					-shifters reloaded during ticks 9, 17, 25, ..., 257

					-NOTE: data for first 2 files is already loaded into shift registers at beginning of scanline (fetched at end of previous scanline)
					  -first fetched tile is tile 3
			
				-during this process, sprite evaluation for the next scanline is happening as a separate process

			* cycles 257 - 320
				-tile data for the sprites on the next scanline are fetched (takes 32 cycles. 2 cycles * 4 bytes * 8 sprites)
				  -garbage nametable byte
				  -garbage nametable byte
					-X positions and attributes for each sprite are loaded from secondary OAM into respective counters/latches here
					  -attribute byte loaded during first tick, x coordinate loaded during the second
				  -tile bitmap low
				  -tile bitmap high

					-garbage fetches occur so BG tile fetch circuitry could be reused
				-if there are less than 8 sprites on the next scanline, dummy fetchesto tile $FF occur
				  -data is then discarded and the dummy sprites are loaded with a transparent bitmap

			* cycles 321 - 336
				-first 2 tiles for the next scanline are fetched and loaded into shift registers (16 cycles. 2 * 4 bytes * 2 tiles)
				  -nametable byte
				  -attribute table byte
				  -tile bitmap low
				  -tile bitmap high

			* cycles 337 - 340
				-2 bytes are fetched. purpose is unknown (2 cycles each. 4 cycles total)
				  -nametable byte
				  -nametable byte

				  -both fetched bytes are the same nametable byte that will be fetched at the beginning of the next scanline (tile 3)
					-MMC5 uses this string of 3 consecutive nametable fetches to clock a scanline counter

		/*** post-render scanline (240) ***
			-PPU idles
			-vblank flag not set yet

		/*** vblank (241 - 260) ***
			-vblank flag of PPU set at tick 1 (second tick) of scanline 241, where the vblank NMI also occurs (if enabled)
			-PPU makes no memory accesses here (safe for program to access the PPU)

		/*** pre-render scanline (261) ***
			-dummy scanline
			-fills shift registers with data for first 2 tiles of next scanline
			-no pixels rendered, but PPU makes same memory accesses it normally would

			-varies in length (unless rendering is disabled)
			  -cycle at end of frame is skipped on odd frames (jumps from (339,261) to (0,0)
			    -replaces the idle tick at cycle 0 of the first visible scanline
			  -done to produce a crisper image on original hardware when the screen isn't scrolling
			    -causes a shift in the NTSC colorburst phase
			  -behavior can be bypassed by keeping rendering disabled until after this scanline has passed
			    -makes image look more interlaced
			  -vertical scroll bits are reloaded during pixels 280 - 304 if rendering is enabled
	*/


	/**** ALT ****/

	/* FOR EACH CYCLE
		IF THIS CYCLE IS A MULTIPLE OF 8
			-load new data into the shift registers

		-fetch bit from background shift register to create pixel (depends on fine x scroll)
		-shift the shift registers once to the data for the next pixel
		-decrement the 8 x-position counters for the sprites

		FOR EACH SPRITE (from highest to lowest priority)
			-if the x-position counter is 0, make the sprite "active" (shift respective pair of shift registers for the sprite once every cycle)
			
			-move first non-transparent pixel onto multiplexer to join BG pixel

			IF SPRITE IS ACTIVE
				IF SPRITE HAS FG PRIORITY OR THE BG PIXEL IS 0
					-output sprite pixel
				IF SPRITE HAS BG PRIORITY AND BG PIXEL IS NONZERO
					-output bg pixel
			-don't output pixel if another, higher priority sprite has an opaque pixel at the same location
	*/
}
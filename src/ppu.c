#include "ppu.h"

void render_pixel(SDL_Surface* screen, int x, int y, uint32_t pixel)
{
	/**((uint32_t*)screen->pixels + (x + y*screen->w)) = pixel; */

	uint32_t* pixels = (uint32_t*)screen->pixels;
	pixels[x + (y*screen->w)] = pixel;
}

/* TODO: FIX ONE PIXEL COLOR BUG */
void render_tile(PPU* ppu, SDL_Surface* screen, int tile, int x, int y)
{
	int row, pixel;
	uint8_t mask;

	uint8_t lb, hb;
	uint32_t color;
	uint32_t r = SDL_MapRGB(screen->format, 255, 0, 0);
	uint32_t g = SDL_MapRGB(screen->format, 0, 255, 0);
	uint32_t b = SDL_MapRGB(screen->format, 0, 0, 255);

	for (row = 0; row < 8; ++row)
	{
		for (pixel = 0; pixel < 8; ++pixel)
		{
			mask = 1 << (7 - pixel);
			lb = (ppu->vram[(tile*16)+row] & mask) >> (7-pixel);
			hb = (ppu->vram[(tile*16)+row+8] & mask) >> (6-pixel);
			color = lb | hb;

			if (color == 1) color = r;
			else if (color == 2) color = g;
			else if (color == 3) color = b;

			render_pixel(screen, x + pixel, y + row, color);
		}
	}
}

void ppu_render_pattern_table(PPU* ppu, SDL_Surface* screen)
{
	int i;

	SDL_LockSurface(screen);

	for (i = 0; i < 256*2; ++i)
	{
		render_tile(ppu, screen, i, (i%16)*8, (i/16)*8);
	}

	SDL_UnlockSurface(screen);
	SDL_Flip(screen);
}
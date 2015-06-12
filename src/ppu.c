#include "ppu.h"

void render_pixel(SDL_Surface* screen, int x, int y, uint32_t pixel)
{
	/**((uint32_t*)screen->pixels + (x + y*screen->w)) = pixel; */

	uint32_t* pixels = (uint32_t*)screen->pixels;
	pixels[x + (y*screen->w)] = pixel;
}

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
			int bit = 7 - pixel;
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

			box.x = (x + pixel)*4;
			box.y = (y + row)*4;
			box.w = 4;
			box.h = 4;
			
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

	for (i = 0; i < 256*1; ++i)
	{
		render_tile(ppu, screen, i, (i%16)*8, (i/16)*8);
	}

	SDL_UnlockSurface(screen);
	SDL_Flip(screen);
}
#include <stdint.h>

#include <SDL.h>

#include "renderer.h"

void renderer_init_surface(RenderSurface* surface, char* title)
{
	*surface = SDL_SetVideoMode(800, 600, 32, SDL_SWSURFACE);
	SDL_WM_SetCaption(title, title);
	/*SDL_Init(SDL_INIT_EVERYTHING);*/
}

void render_pixel(RenderSurface screen, int x, int y, uint32_t color)
{
	SDL_Rect box;
	uint8_t scale = 2;
	box.x = x*scale;
	box.y = y*scale;
	box.w = box.h = scale;

	SDL_FillRect(screen, &box, color);

	/*((uint32_t*)screen->pixels)[x + (y*screen->w)] = color;*/
}

void renderer_flip_surface(RenderSurface screen)
{
	/* TODO: better frame limiting */
	static uint32_t last_ms = 0;
	SDL_Flip(screen);
	while ((SDL_GetTicks() - last_ms) < 1000/60);
	last_ms = SDL_GetTicks();
}
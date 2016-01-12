#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include <SDL.h>

typedef SDL_Surface* RenderSurface;

void renderer_init_surface(RenderSurface* surface, char* title);
void render_pixel(RenderSurface screen, int x, int y, uint32_t color);
void renderer_flip_surface(RenderSurface screen);

#endif
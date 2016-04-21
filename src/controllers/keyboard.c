#include <stdint.h>
#include <SDL.h>

#include "../controller.h"

/* TODO: make this configurable */

#define KEY_A		SDLK_x
#define KEY_B		SDLK_z
#define KEY_SELECT	SDLK_RSHIFT
#define KEY_START	SDLK_RETURN
#define KEY_UP		SDLK_UP
#define KEY_DOWN	SDLK_DOWN
#define KEY_LEFT	SDLK_LEFT
#define KEY_RIGHT	SDLK_RIGHT

static uint8_t poll()
{
	/* Poll the keyboard and return the state of relevant keys */

	uint8_t* keys = SDL_GetKeyState(NULL);

	/* Use 0 for pressed button because this returned value
	   will be inverted before being returned to the CPU */
	return ~(keys[KEY_A] |
		   (keys[KEY_B] << 1) |
		   (keys[KEY_SELECT] << 2) |
		   (keys[KEY_START] << 3) |
		   (keys[KEY_UP] << 4) |
		   (keys[KEY_DOWN] << 5) |
		   (keys[KEY_LEFT] << 6) |
		   (keys[KEY_RIGHT] << 7));
}

void controller_kb_init(Controller* c)
{
	c->poll = poll;
}
#include <stdint.h>
#include "controller.h"

static uint8_t input = 0;

void controllers_write_input(uint8_t val)
{
	/* TODO: proper writes. Open bus (usually the most
	   significant byte of address of controller port
	   (e.g., 0x40); Paper Boy relies on it) */
	input = val;
}

uint8_t controller_read_output(Controller* c)
{
	/* TODO: open bus */

	/* Invert controller state to turn 0->1 for button
	   presses. Emulates official standard controllers
	   which return 1 after all buttons have been read */
	uint8_t val = ~c->state & 1;
	c->state >>= 1;
	return val;
}

void controller_update(Controller* c)
{
	/* Strobe */
	if (input & 1)
		c->state = c->poll();
}
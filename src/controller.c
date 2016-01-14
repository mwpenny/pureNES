#include <stdint.h>
#include "controller.h"

static uint8_t input = 0;

void controllers_write_input(uint8_t val)
{
	/* TODO: proper writes. Open bus */
	input = val;
}

uint8_t controller_read_output(Controller* c)
{
	/* TODO: return 1 after empty */
	/* TODO: open bus */
	uint8_t val = c->state & 1;
	c->state >>= 1;
	return val;
}

void controller_update(Controller* c)
{
	/* Strobe */
	if (input & 1)
		c->state = c->poll();
}
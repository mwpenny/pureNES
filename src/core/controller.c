#include <stdint.h>
#include "controller.h"

void controller_init(Controller* c)
{
	c->output = c->state = 0xFF;
	c->input = 0;
}

void controller_write_input(Controller* c, uint8_t val)
{
	/* TODO: proper writes. Open bus (usually the most
	   significant byte of address of controller port
	   (e.g., 0x40); Paper Boy relies on it) */
	c->input = val;
}

void controller_update(Controller* c)
{
	if (c->input & 1)
		c->output = c->state;
}

uint8_t controller_read_output(Controller* c)
{
	/* TODO: open bus */

	/* Invert controller state to turn 0->1 for button
	   presses. Emulates official standard controllers
	   which return 1 after all buttons have been read */
	uint8_t val = ~c->output & 1;
	c->output >>= 1;
	return val;
}

void controller_set_button(Controller* c, ControllerButton btn, uint8_t pressed)
{
	/* Use 0 for pressed buttons because the returned value
   	   will be inverted before being returned to the game code */
	if (pressed)
		c->state &= ~btn;
	else
		c->state |= btn;
}
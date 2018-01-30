#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>

typedef enum {
	CONTROLLER_NONE = 0,
	CONTROLLER_A = 1,
	CONTROLLER_B = 2,
	CONTROLLER_SELECT = 4,
	CONTROLLER_START = 8,
	CONTROLLER_UP = 16,
	CONTROLLER_DOWN = 32,
	CONTROLLER_LEFT = 64,
	CONTROLLER_RIGHT = 128
} ControllerButton;

typedef struct {
	uint8_t state, output;
	uint8_t input;
} Controller;

void controller_init(Controller* c);
void controller_set_button(Controller* c, ControllerButton btn, uint8_t pressed);
void controller_update(Controller* c);
void controller_write_input(Controller* c, uint8_t val);
uint8_t controller_read_output(Controller* c);

#endif

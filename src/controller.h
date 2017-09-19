#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>

typedef struct {
	uint8_t state;
	uint8_t (*poll)();
} Controller;

void controller_update(Controller* c);
void controllers_write_input(uint8_t val);
uint8_t controller_read_output(Controller* c);

#endif

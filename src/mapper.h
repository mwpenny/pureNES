#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>

struct Game;
typedef struct {
	void (*init)(struct Game* game);
} Mapper;

void mapper_register(uint8_t mapper_num, Mapper* mapper);
Mapper* mapper_get(uint8_t mapper_num);

#endif
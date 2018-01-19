#ifndef APU_H
#define APU_H

#include <stdint.h>

typedef struct {
	uint16_t value;
	uint16_t period;
} Timer;

typedef struct {
	Timer timer;
	uint8_t enabled;
	uint8_t negate;
	uint8_t shift;
	uint8_t reload;
	uint16_t target;
} Sweep;

typedef struct {
	Timer timer;
	uint8_t enabled;
	uint8_t looping;
	uint8_t start;
	uint8_t decay_vol;
} Envelope;

typedef struct {
	uint8_t enabled;
	uint8_t value;
} LengthCounter;

typedef struct {
	Timer timer;
	Sweep sweep;
	Envelope env;
	LengthCounter lc;
	uint8_t enabled;
	uint8_t duty;
	uint8_t phase;
} PulseChannel;

typedef struct {
	Timer timer;
	Envelope env;
	LengthCounter lc;
	uint16_t lfsr;
	uint8_t enabled;
	uint8_t mode;
} NoiseChannel;

typedef enum {
	FC_4STEP = 0,
	FC_5STEP
} FCSequence;

typedef struct {
	struct NES* nes;
	PulseChannel pulse1, pulse2;
	NoiseChannel noise;
	FCSequence fc_sequence;
	uint8_t fc_irq_enabled;

	uint32_t sample_buf_size;
	uint32_t sample_buf_insert_pos;
	uint32_t sample_buf_read_pos;
	uint16_t *sample_buf1, *sample_buf2;
	uint16_t *current_read_buf, *current_write_buf;
	uint32_t read_buffer_filled;
	uint32_t cycles;
} APU;

int apu_init(APU* apu, struct NES* nes);
void apu_cleanup(APU* apu);
void apu_write(APU* apu, uint16_t addr, uint8_t val);
uint8_t apu_read (APU* apu, uint16_t addr);
void apu_tick(APU* apu);

#endif

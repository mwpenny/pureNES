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
	Sweep sweep;
	uint8_t duty_cycle;
	uint8_t phase;

	uint8_t length_counter;
	uint8_t lc_halted;
	uint8_t using_const_vol;
	uint8_t vol_env;
} PulseChannel;

typedef enum {
	FC_4STEP = 0,
	FC_5STEP
} FCSequence;

typedef struct {
	struct NES* nes;
	PulseChannel pulse1, pulse2;
	FCSequence fc_sequence;
	uint8_t interrupt_enabled;

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

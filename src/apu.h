#ifndef APU_H
#define APU_H

#include <stdint.h>

/* loop env flag naming */
/* env vol/period naming */

typedef struct {
	uint8_t duty;
	uint8_t lc_enabled;
	uint8_t env_enabled;
	uint8_t volume;

	uint8_t sweep_enabled;
	uint8_t sweep_period;
	uint8_t sweep_negate;
	uint8_t sweep_shift;

	uint16_t timer;
	uint8_t lc_load;
} PulseChannel;

typedef struct {
	uint8_t lc_enabled;
	uint8_t lin_counter_reload;

	uint16_t timer;
	uint8_t lc_load;	
} TriangleChannel;

typedef struct {
	uint8_t lc_enabled;
	uint8_t env_enabled;
	uint8_t volume;

	uint8_t loop_enabled;
	uint8_t period;

	uint8_t lc_load;	
} NoiseChannel;

typedef struct {
	uint8_t irq_enabled;
	uint8_t loop_enabled;
	uint8_t freq_idx;

	uint8_t direct_load;

	uint8_t sample_addr;

	uint8_t sample_len;
} DMCChannel;

typedef struct {
	PulseChannel pulse1, pulse2;
	TriangleChannel triangle;
	NoiseChannel noise;
	DMCChannel dmc;

	uint8_t fc_seq_mode;
} APU;


void apu_init(APU* apu);
void apu_write(APU* apu, uint16_t addr, uint8_t val);
uint8_t apu_read (APU* apu, uint16_t addr);


#endif
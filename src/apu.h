#ifndef APU_H
#define APU_H

#include <stdint.h>

/* loop env flag naming */
/* env vol/period naming */

typedef struct {
	uint16_t period;
	uint16_t value;
} Timer;

typedef struct {
	uint8_t duty;
	uint8_t phase;
	uint8_t lc_enabled;
	uint8_t env_enabled;
	uint8_t volume;

	uint8_t sweep_enabled;
	uint8_t sweep_period;
	uint8_t sweep_negate;
	uint8_t sweep_shift;

	Timer timer;
	uint8_t lc_load;
} PulseChannel;

typedef struct {
	uint8_t lc_enabled;
	uint8_t lin_counter_reload;

	Timer timer;
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
	uint16_t sample_rate;
	int cycle;
} APU;

void apu_genwave();
void apu_init(APU* apu);
void apu_cleanup(APU* apu);
void apu_write(APU* apu, uint16_t addr, uint8_t val);
uint8_t apu_read (APU* apu, uint16_t addr);
void apu_tick(APU* apu);


#endif
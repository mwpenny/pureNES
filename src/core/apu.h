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
	uint8_t reload;
	uint8_t control;
	Timer timer;
} LinearCounter;

typedef struct {
	Timer timer;
	LengthCounter lc;
	LinearCounter lin_ctr;
	uint8_t enabled;
	uint8_t phase;
} TriangleChannel;

typedef struct {
	Timer timer;
	Envelope env;
	LengthCounter lc;
	uint16_t lfsr;
	uint8_t enabled;
	uint8_t mode;
} NoiseChannel;

typedef struct {
	Timer timer;

	uint8_t irq_enabled;
	uint8_t irq_fired;
	uint8_t loop;
	uint8_t sample_buf;
	uint8_t sample_buf_filled;
	uint8_t shift_reg;
	uint8_t bits_remaining;

	uint8_t output;
	uint8_t silence;

	uint16_t curr_addr;
	uint16_t bytes_remaining;

	uint16_t sample_addr;
	uint16_t sample_len;
} DMCChannel;

typedef enum {
	FC_4STEP = 0,
	FC_5STEP
} FCSequence;

typedef void (*SoundCallback)(uint16_t* read_buf, uint32_t buf_size, void* userdata);

struct NES;
struct NESInitInfo;

typedef struct {
	struct NES* nes;
	SoundCallback snd_cb;
	void* snd_userdata;

	PulseChannel pulse1, pulse2;
	TriangleChannel triangle;
	NoiseChannel noise;
	DMCChannel dmc;
	FCSequence fc_sequence;
	uint8_t fc_irq_enabled;
	uint8_t fc_irq_fired;
	uint8_t fc_reset_delay;

	uint32_t sample_buf_size;
	uint32_t sample_buf_insert_pos;
	uint16_t *sample_buf1, *sample_buf2;
	uint16_t *current_read_buf, *current_write_buf;
	uint32_t cycles;
} APU;

int apu_init(APU* apu, struct NES* nes, struct NESInitInfo* init_info);
void apu_cleanup(APU* apu);
void apu_write(APU* apu, uint16_t addr, uint8_t val);
uint8_t apu_read (APU* apu, uint16_t addr);
void apu_tick(APU* apu);

#endif

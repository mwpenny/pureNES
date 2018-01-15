#include <errno.h>
#include <math.h>
#include <string.h>

#include <SDL.h>

#include "apu.h"

/* Length counter load values */
static const uint8_t LC_INIT_VALUES[32] = {
	10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
	12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

static const uint8_t PULSE_DUTY_CYCLES[4] = {
	0x40,  /* 01000000 (12.5%) */
	0x60,  /* 01100000 (25%) */
	0x78,  /* 01111000 (50%) */
	0x9F   /* 10011111 (75%) */
};

SDL_mutex* mutex;

void audio_callback(void* userdata, uint8_t* stream, int len)
{
	APU* apu = (APU*)userdata;
	int i;

	uint16_t* buf = (uint16_t*)stream;
	len /= 2;

	SDL_LockMutex(mutex);
	for (i = 0; i < len; ++i)
	{
		buf[i] = apu->current_read_buf[apu->sample_buf_read_pos];
		apu->sample_buf_read_pos = (apu->sample_buf_read_pos + 1) % apu->sample_buf_size;
	}
	SDL_UnlockMutex(mutex);
}

int apu_init(APU* apu, struct NES* nes)
{
	/* TODO: abstract out audio init, return pointer to audio callback */
	SDL_AudioSpec desired, obtained;
	memset(apu, 0, sizeof(*apu));
	apu->nes = nes;

	/* TODO: proper downsampling (a real NES outputs ~1789773 samples/second) */
	desired.freq = 44100;
	desired.format = AUDIO_U16SYS;
	desired.channels = 1;  /* TODO: stereo experimentation */
	desired.samples = 2048;
	desired.callback = audio_callback;
	desired.userdata = apu;
	if (SDL_OpenAudio(&desired, &obtained) != 0)
	{
		fprintf(stderr, "Error: could not open audio device (%d)\n", errno);
		return -1;
	}
	apu->sample_buf_size = (obtained.size / sizeof(uint16_t)) * 4;
	if (!(apu->sample_buf1 = (uint16_t*)malloc(apu->sample_buf_size * sizeof(uint16_t))))
	{
		fprintf(stderr, "Error: could not allocate audio buffer 1 (%d)\n", errno);
		return -1;
	}
	memset(apu->sample_buf1, 0, apu->sample_buf_size);
	if (!(apu->sample_buf2 = (uint16_t*)malloc(apu->sample_buf_size * sizeof(uint16_t))))
	{
		fprintf(stderr, "Error: could not allocate audio buffer 2 (%d)\n", errno);
		free(apu->sample_buf1);
		return -1;
	}
	memset(apu->sample_buf2, 0, apu->sample_buf_size);

	if (!(mutex = SDL_CreateMutex()))
	{
		fprintf(stderr, "Error: could not create mutex for APU audio buffer access (%d)\n", errno);
		free(apu->sample_buf1);
		free(apu->sample_buf2);
		return -1;
	}

    SDL_PauseAudio(0);
	apu->current_read_buf = apu->sample_buf1;
	apu->current_write_buf = apu->sample_buf2;
	return 0;
}


void apu_cleanup(APU* apu)
{
	/* TODO */
}

static void update_sweep_target(APU* apu, PulseChannel* channel)
{
	/* Calculate change amount for channel's period */
	Sweep* sweep = &channel->sweep;
	int16_t c = channel->timer.period >> sweep->shift;
	if (sweep->negate)
	{
		c = -c;
		if (channel == &apu->pulse1)
			--c;
	}
	sweep->target = channel->timer.period + c;
}

static void clock_sweep(APU* apu, PulseChannel* channel)
{
	Sweep* sweep = &channel->sweep;

	if (sweep->timer.value-- == 0)
	{
		sweep->timer.value = sweep->timer.period;
		update_sweep_target(apu, channel);

		/* Only update the channel if the target period is in range (not muted) */
		if (sweep->enabled && sweep->shift > 0 && channel->timer.period >= 8 && sweep->target <= 0x7FF)
			channel->timer.period = sweep->target;
	}

	if (sweep->reload)
	{
		sweep->timer.value = sweep->timer.period;
		sweep->reload = 0;
	}
}

static void clock_lc(PulseChannel* channel)
{
	if (!channel->lc_halted && channel->lc != 0)
		--channel->lc;
}

static void clock_env(PulseChannel* channel)
{
	Envelope* env = &channel->env;
	if (env->start)
	{
		env->decay = 15;
		env->timer.period = channel->vol_env;
		env->start = 0;
	}
	else if (env->timer.value-- == 0)
	{
		env->timer.value = channel->vol_env;
		if (env->decay > 0)
		{
			--env->decay;
		}
		else if (channel->lc_halted)
		{
			env->decay = 15;
		}
	}
}

static void clock_sequencer(APU* apu)
{
	/* Steps common to both the 4-step and 5-step sequence */
	switch (apu->cycles)
	{
		case 7457:   /* Step 1 (after 3728.5 APU cycles) */
			/* TODO: clock triangle's linear counter */
			clock_env(&apu->pulse1);
			clock_env(&apu->pulse2);
			break;
		case 14913:  /* Step 2 (after 7456.5 APU cycles) */
			/* TODO: clock envelopes and triangle's linear counter */
			clock_sweep(apu, &apu->pulse1);
			clock_sweep(apu, &apu->pulse2);
			clock_lc(&apu->pulse1);
			clock_lc(&apu->pulse2);
			break;
		case 22371:  /* Step 3 (after 11185.5 APU cycles) */
			/* TODO: clock triangle's linear counter */
			clock_env(&apu->pulse1);
			clock_env(&apu->pulse2);
			break;
	}
	if (apu->fc_sequence == FC_4STEP)
	{
		switch (apu->cycles)
		{
			case 29828:  /* First and third parts of step 4 (14914/14915 APU cycles) */
				/* TODO: set frame interrupt flag if interupt inhibit is clear */
				break;
			case 29829:  /* Second part of step 4 (14914.5 APU cycles) */
				/* TODO: clock triangle's linear counter */
				/* TODO: set frame interrupt flag if interupt inhibit is clear */
				clock_env(&apu->pulse1);
				clock_env(&apu->pulse2);
				clock_sweep(apu, &apu->pulse1);
				clock_sweep(apu, &apu->pulse2);
				clock_lc(&apu->pulse1);
				clock_lc(&apu->pulse2);
				break;
			case 29830:
				/* TODO: set frame interrupt flag if interupt inhibit is clear */
				apu->cycles = 0;
		}
	}
	else
	{
		switch (apu->cycles)
		{
			case 37281:  /* Step 5 (18640.5 APU cycles) */
				/* TODO: clock triangle's linear counter */
				clock_env(&apu->pulse1);
				clock_env(&apu->pulse2);
				clock_sweep(apu, &apu->pulse1);
				clock_sweep(apu, &apu->pulse2);
				clock_lc(&apu->pulse1);
				clock_lc(&apu->pulse2);
				break;
			case 37282:
				apu->cycles = 0;
		}
	}
}

/* 7  bit  0
   ---- ----
   DDLC VVVV
   |||| ||||
   |||| ++++- Volume/envelope (toggled by bit 5)
   |||+------ Controls whether lower 4 bits are constant volume or
   |||        envelope period
   ||+------- Length counter halt (when set, pauses/silences channel)
   ++-------- Duty cycle select */
static void pulse_flags1_write(APU* apu, PulseChannel* channel, uint8_t val)
{
	channel->duty_cycle = PULSE_DUTY_CYCLES[(val >> 6) & 3];
	channel->lc_halted = (val >> 5) & 1;
	channel->using_const_vol = (val >> 4) & 1;
	channel->vol_env = val & 0xF;
	update_sweep_target(apu, channel);
}

static void pulse_sweep_write(APU* apu, PulseChannel* channel, uint8_t val)
{
	Sweep* sweep = &channel->sweep;
	sweep->enabled = val >> 7;
	sweep->timer.period = (val >> 4) & 7;
	sweep->negate = (val >> 3) & 1;
	sweep->shift = val & 7;
	sweep->reload = 1;
	update_sweep_target(apu, channel);
}

/* Pulse timer low byte ($4002/$4006) */
static void pulse_flags3_write(APU* apu, PulseChannel* channel, uint8_t val)
{
	channel->timer.period = (channel->timer.period & 0x700) | val;
	update_sweep_target(apu, channel);
}

/* 7  bit  0
   ---- ----
   LLLL LTTT
   |||| ||||
   |||| |+++- Timer high bits
   ++++ +---- Index of length counter load value */
static void pulse_flags4_write(PulseChannel* channel, uint8_t val)
{
	channel->timer.period = (channel->timer.period & 0xFF) | ((val & 7) << 8);
	if (channel->enabled)
		channel->lc = LC_INIT_VALUES[(val >> 3) & 0x1F];
	channel->env.start = 1;
	//channel->phase = 0;
}

static void status_write(APU* apu, uint8_t val)
{
	/* TODO: enable/disable channels */
	/* TODO: silence all channels on power up and reset (i.e., write 0) */

	apu->pulse1.enabled = val & 1;
	apu->pulse2.enabled = (val >> 1) & 1;

	if (!apu->pulse1.enabled)
		apu->pulse1.lc = 0;
	if (!apu->pulse2.enabled)
		apu->pulse2.lc = 0;
}

static uint8_t status_read(APU* apu)
{
	/* TODO: also return DMC interrupt, frame interrupt, DMC active */
	return ((apu->pulse2.lc > 0) << 1) | (apu->pulse1.lc > 0);
}

static uint8_t clock_pulse(PulseChannel* channel)
{
	/* Clock pulse waveform generator */
	uint8_t vol = 0;
	if (channel->timer.value-- == 0)
	{
		channel->timer.value = channel->timer.period;
		channel->phase = (channel->phase + 1) & 7;
	}
	if (channel->timer.period < 8 || channel->sweep.target > 0x7FF || channel->lc == 0)
		return 0;
	vol = channel->using_const_vol ? channel->vol_env : channel->env.decay;
	return vol * ((channel->duty_cycle >> (7 - channel->phase)) & 1);
}

/* 7  bit  0
   ---- ----
   MI-- ----
   |+-------- Interrupt inhibit flag
   +--------- Sequencer mode (0: 4-step, 1: 5-step) */
static void write_sequencer(APU* apu, uint8_t val)
{
	/* TODO: reset divider and sequencer */
	/* TODO: After 3 or 4 CPU clock cycles*, the timer is reset */
	/* TODO: If the mode flag is set, then both "quarter frame" and
	   "half frame" signals are also generated. */

	/* TODO: If the write occurs during an APU cycle, the effects occur
	   3 CPU cycles after the $4017 write cycle, and if the write occurs
	   between APU cycles, the effects occurs 4 CPU cycles after the write cycle */
	apu->fc_sequence = (FCSequence)(val >> 7);
	apu->interrupt_enabled = (val >> 6) & 1;

	if (val & 0x80)
	{
		/* TODO: also generate "quarter frame" signal */
		clock_sweep(apu, &apu->pulse1);
		clock_sweep(apu, &apu->pulse2);
		clock_lc(&apu->pulse1);
		clock_lc(&apu->pulse2);
	}
}

void apu_write(APU* apu, uint16_t addr, uint8_t val)
{
	/* TODO: implement all registers */
	/* TODO: optimize (look up tables may be useful for units with the same
	   types of registers (i.e., pulse) */
	switch (addr)
	{
		case 0x4000:
			pulse_flags1_write(apu, &apu->pulse1, val);
			break;
		case 0x4001:
			pulse_sweep_write(apu, &apu->pulse1, val);
			break;
		case 0x4002:
			pulse_flags3_write(apu, &apu->pulse1, val);
			break;
		case 0x4003:
			pulse_flags4_write(&apu->pulse1, val);
			break;
		case 0x4004:
			pulse_flags1_write(apu, &apu->pulse2, val);
			break;
		case 0x4005:
			pulse_sweep_write(apu, &apu->pulse2, val);
			break;
		case 0x4006:
			pulse_flags3_write(apu, &apu->pulse2, val);
			break;
		case 0x4007:
			pulse_flags4_write(&apu->pulse2, val);
			break;
		case 0x4015:
			status_write(apu, val);
			break;
		case 0x4017:
			write_sequencer(apu, val);
			break;
	}
}

uint8_t apu_read(APU* apu, uint16_t addr)
{
	/* TODO */
	switch (addr)
	{
		case 0x4015:
			return status_read(apu);
	}
	return 0;
}

void apu_tick(APU* apu)
{
	/* TODO: safe threading */

	clock_sequencer(apu);

	if ((apu->cycles % 2) == 0)
	{
		uint16_t val = 100 * (clock_pulse(&apu->pulse1) + clock_pulse(&apu->pulse2));

		if ((apu->cycles % 40) == 0)
		{
			apu->current_write_buf[apu->sample_buf_insert_pos] = val;
			apu->sample_buf_insert_pos = (apu->sample_buf_insert_pos + 1) % apu->sample_buf_size;
			if (apu->sample_buf_insert_pos == 0)
			{
				uint16_t* tmp = apu->current_write_buf;
				apu->current_write_buf = apu->current_read_buf;
				//printf("Audio buffer full\n");

				SDL_LockMutex(mutex);
				apu->current_read_buf = tmp;
				apu->sample_buf_read_pos = 0;
				SDL_UnlockMutex(mutex);
			}
		}
	}
	++apu->cycles;

	/*
		1) Clock main divider (~240Hz)
			-Clocks sequencer (controls length counters, sweep units, the
			 unit's linear counter, and generates IRQs) */
}
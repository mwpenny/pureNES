#include <errno.h>
#include <math.h>
#include <string.h>

#include <SDL.h>

#include "apu.h"
#include "cpu.h"
#include "nes.h"

static const uint8_t PULSE_DUTY_CYCLES[4] = {
	0x40,  /* 01000000 (12.5%) */
	0x60,  /* 01100000 (25%) */
	0x78,  /* 01111000 (50%) */
	0x9F   /* 10011111 (75%) */
};

/* Length counter load values */
static const uint8_t LC_INIT_VALUES[32] = {
	10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
	12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

static const uint8_t TRI_SEQUENCE[32] = {
	15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
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
	if (!(apu->sample_buf1 = (uint16_t*)calloc(apu->sample_buf_size, sizeof(uint16_t))))
	{
		fprintf(stderr, "Error: could not allocate audio buffer 1 (%d)\n", errno);
		return -1;
	}
	if (!(apu->sample_buf2 = (uint16_t*)calloc(apu->sample_buf_size, sizeof(uint16_t))))
	{
		fprintf(stderr, "Error: could not allocate audio buffer 2 (%d)\n", errno);
		free(apu->sample_buf1);
		return -1;
	}

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

	apu->noise.lfsr = 1;
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

static void sweep_clock(APU* apu, PulseChannel* channel)
{
	Sweep* sweep = &channel->sweep;
	uint8_t reset = sweep->reload;
	if (sweep->timer.value-- == 0)
	{
		/* Only update the channel if the target period is in range (not muted) */
		if (sweep->enabled &&
			sweep->shift > 0 &&
			channel->timer.period >= 8 &&
			sweep->target <= 0x7FF)
		{
			channel->timer.period = sweep->target;
		}
		reset = 1;
	}
	if (reset)
	{
		sweep->timer.value = sweep->timer.period;
		sweep->reload = 0;
		update_sweep_target(apu, channel);
	}
}

static void lc_clock(LengthCounter* lc)
{
	if (lc->enabled && lc->value != 0)
		--lc->value;
}

static void env_clock(Envelope* env)
{
	if (env->start)
	{
		env->decay_vol = 15;
		env->start = 0;
	}
	else if (env->timer.value-- == 0)
	{
		env->timer.value = env->timer.period;
		if (env->decay_vol > 0)
			--env->decay_vol;
		else if (env->looping)
			env->decay_vol = 15;
	}
}

static void lin_ctr_clock(LinearCounter* lin_ctr)
{
	if (lin_ctr->reload)
		lin_ctr->timer.value = lin_ctr->timer.period;
	else if (lin_ctr->timer.value > 0)
		--lin_ctr->timer.value;

	if (!lin_ctr->control)
		lin_ctr->reload = 0;
}

static void fc_qframe(APU* apu)
{
	lin_ctr_clock(&apu->triangle.lin_ctr);
	env_clock(&apu->pulse1.env);
	env_clock(&apu->pulse2.env);
}

static void fc_hframe(APU* apu)
{
	sweep_clock(apu, &apu->pulse1);
	sweep_clock(apu, &apu->pulse2);
	lc_clock(&apu->pulse1.lc);
	lc_clock(&apu->pulse2.lc);
	lc_clock(&apu->triangle.lc);
	lc_clock(&apu->noise.lc);
}

static void fc_clock(APU* apu)
{
	/* Steps common to both the 4-step and 5-step sequence */
	switch (apu->cycles)
	{
		case 7457:   /* Step 1 (after 3728.5 APU cycles) */
		case 22371:  /* Step 3 (after 11185.5 APU cycles) */
			fc_qframe(apu);
			return;
		case 14913:  /* Step 2 (after 7456.5 APU cycles) */
			fc_hframe(apu);
			return;
	}
	if (apu->fc_sequence == FC_4STEP)
	{
		switch (apu->cycles)
		{
			case 29829:  /* Second part of step 4 (14914.5 APU cycles) */
				fc_qframe(apu);
				fc_hframe(apu);
				return;
			case 29828:  /* First and third parts of step 4 (14914/14915 APU cycles) */
			case 29830:
				if (apu->fc_irq_enabled)
					cpu_fire_interrupt(&apu->nes->cpu, INT_IRQ);
				apu->cycles %= 29830;
				return;
		}
	}
	else
	{
		switch (apu->cycles)
		{
			case 37281:  /* Step 5 (18640.5 APU cycles) */
				fc_qframe(apu);
				fc_hframe(apu);
				return;
			case 37282:
				apu->cycles = 0;
		}
	}
}

static uint8_t pulse_clock(PulseChannel* channel)
{
	/* Clock pulse waveform generator */
	uint8_t vol = 0;
	if (channel->timer.value-- == 0)
	{
		channel->timer.value = channel->timer.period;
		channel->phase = (channel->phase + 1) & 7;
	}
	if (channel->timer.period < 8 || channel->sweep.target > 0x7FF || channel->lc.value == 0)
		return 0;
	vol = channel->env.enabled ? channel->env.decay_vol : channel->env.timer.period;
	return vol * ((channel->duty >> (7 - channel->phase)) & 1);
}

static uint8_t triangle_clock(TriangleChannel* channel)
{
	/* Triangle waveform generator */
	uint8_t counters_active = channel->lc.value && channel->lin_ctr.timer.value;
	if (channel->timer.value-- == 0)
	{
		channel->timer.value = channel->timer.period;
		if (counters_active)
			channel->phase = (channel->phase + 1) & 0x1F;
	}
	/*if (!counters_active)
		return 0;*/
	return TRI_SEQUENCE[channel->phase];
}

static uint8_t noise_clock(NoiseChannel* channel)
{
	/* Noise waveform generator */
	if (channel->timer.value-- == 0)
	{
		uint8_t fb = channel->lfsr & 1;
		channel->timer.value = channel->timer.period;
		fb ^= channel->mode ? ((channel->lfsr >> 6) & 1) : (channel->lfsr >> 1) & 1;
		channel->lfsr = ((channel->lfsr >> 1) & 0x3FFF) | fb << 14;
	}
	if ((channel->lfsr & 1) && channel->lc.value > 0)
		return channel->env.enabled ? channel->env.decay_vol : channel->env.timer.period;
	return 0;
}


/* 7  bit  0
   ---- ----
   DDLC VVVV
   |||| ||||
   |||| ++++- Volume/envelope (toggled by bit 5)
   |||+------ Controls whether V is constant volume or envelope period
   ||+------- Length counter halt (when set, pauses/silences channel)
   ||         and envelope loop control
   ++-------- Duty cycle select */
static void pulse_flags1_write(PulseChannel* channel, uint8_t val)
{
	channel->duty = PULSE_DUTY_CYCLES[(val >> 6) & 3];
	channel->env.looping = (val >> 5) & 1;
	channel->lc.enabled = !channel->env.looping;
	channel->env.enabled = !((val >> 4) & 1);
	channel->env.timer.period = val & 0xF;
}

/* 7  bit  0
   ---- ----
   EPPP NSSS
   |||| ||||
   |||| |+++- Shift count
   |||| +---- Negate flag (0: sweep toward lower; 1: sweep toward higher)
   |+++------ Sweep unit period
   +--------- Eman;Enabled flag */
static void pulse_sweep_write(PulseChannel* channel, uint8_t val)
{
	Sweep* sweep = &channel->sweep;
	sweep->enabled = val >> 7;
	sweep->timer.period = (val >> 4) & 7;
	sweep->negate = (val >> 3) & 1;
	sweep->shift = val & 7;
	sweep->reload = 1;
}

/* Pulse timer low byte ($4002/$4006) */
static void pulse_flags3_write(PulseChannel* channel, uint8_t val)
{
	channel->timer.period = (channel->timer.period & 0x700) | val;
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
		channel->lc.value = LC_INIT_VALUES[(val >> 3) & 0x1F];
	channel->env.start = 1;
	channel->phase = 0;
}

/* 7  bit  0
   ---- ----
   CRRR RRRR
   |||| ||||
   |++++++++- Linear counter reload value
   +--------- Countrol flag and length counter halt */
static void triangle_lin_ctr_write(TriangleChannel* channel, uint8_t val)
{
	channel->lin_ctr.control = (val >> 7) & 1;
	channel->lc.enabled = !channel->lin_ctr.control;
	channel->lin_ctr.timer.period = val & 0x7F;
}

/* Triangle timer low byte */
static void triangle_flags2_write(TriangleChannel* channel, uint8_t val)
{
	channel->timer.period = (channel->timer.period & 0x700) | val;
}

/* 7  bit  0
   ---- ----
   LLLL LTTT
   |||| ||||
   |||| |+++- Timer high bits
   ++++ +---- Index of length counter load value */
static void triangle_flags3_write(TriangleChannel* channel, uint8_t val)
{
	channel->timer.period = (channel->timer.period & 0xFF) | ((val & 7) << 8);
	if (channel->enabled)
		channel->lc.value = LC_INIT_VALUES[(val >> 3) & 0x1F];
	channel->lin_ctr.reload = 1;
}


/* 7  bit  0
   ---- ----
   --LC VVVV
     || ||||
     || ++++- Volume/envelope (toggled by bit 5)
     |+------ Controls whether V is constant volume or envelope period
     +------- Length counter halt (when set, pauses/silences channel)
              and envelope loop control */
static void noise_flags1_write(NoiseChannel* channel, uint8_t val)
{
	channel->env.looping = (val >> 5) & 1;
	channel->lc.enabled = !channel->env.looping;
	channel->env.enabled = !((val >> 4) & 1);
	channel->env.timer.period = val & 0xF;
}

/* 7  bit  0
   ---- ----
   M--- PPPP
   |    ||||
   |    ++++- Noise period
   +--------- Mode flag (0: long sequence; 1: short sequence) */
static void noise_flags2_write(NoiseChannel* channel, uint8_t val)
{
	channel->timer.period = val & 0xF;
	channel->mode = (val >> 7) & 1;
}

/* 7  bit  0
   ---- ----
   LLLL L---
   |||| |
   ++++ +--- Index of length counter load value */
static void noise_flags3_write(NoiseChannel* channel, uint8_t val)
{
	if (channel->enabled)
		channel->lc.value = LC_INIT_VALUES[(val >> 3) & 0x1F];
	channel->env.start = 1;
}


/* 7  bit  0
   ---- ----
   ---D NT21
      + ++++- Enables/disables pulse1, pulse2, triangle, noise, or
			  DMC channels, respctively */
static void status_write(APU* apu, uint8_t val)
{
	apu->pulse1.enabled = val & 1;
	apu->pulse2.enabled = (val >> 1) & 1;
	apu->triangle.enabled = (val >> 2) & 1;
	apu->noise.enabled = (val >> 3) & 1;

	/* Disabling a channel sets its length counter to 0, effectively silencing it */
	if (!apu->pulse1.enabled)
		apu->pulse1.lc.value = 0;
	if (!apu->pulse2.enabled)
		apu->pulse2.lc.value = 0;
	if (!apu->triangle.enabled)
		apu->triangle.lc.value = 0;
	if (!apu->noise.enabled)
		apu->noise.lc.value = 0;
}

/* 7  bit  0
   ---- ----
   IF-D NT21
   || | ++++- Set if the respective pulse1, pulse2, triangle, or noise
   || |       length counter value is positive
   || +------ Set if there are DMC bytes remaining
   |+-------- Old status of the frame interrupt flag
   +--------- Status of the DMC interrupt flag */
static uint8_t status_read(APU* apu)
{
	/* TODO: also return DMC interrupt, DMC active */
	uint8_t irq_status = apu->fc_irq_enabled && cpu_interrupt_status(&apu->nes->cpu, INT_IRQ);
	cpu_clear_interrupt(&apu->nes->cpu, INT_IRQ);
	return ((apu->noise.lc.value > 0) << 3) |
		   ((apu->triangle.lc.value > 0) << 2) |
		   ((apu->pulse2.lc.value > 0) << 1) |
		   (apu->pulse1.lc.value > 0) |
		   (irq_status << 6);
}

/* 7  bit  0
   ---- ----
   MI-- ----
   |+-------- Interrupt inhibit flag
   +--------- Frame counter sequence (0: 4-step, 1: 5-step) */
static void fc_write(APU* apu, uint8_t val)
{
	/* TODO: reset divider and sequencer */
	/* TODO: After 3 or 4 CPU clock cycles*, the timer is reset */

	/* TODO: If the write occurs during an APU cycle, the effects occur
	   3 CPU cycles after the $4017 write cycle, and if the write occurs
	   between APU cycles, the effects occurs 4 CPU cycles after the write cycle */

	apu->fc_sequence = (FCSequence)(val >> 7);
	apu->fc_irq_enabled = (val & 0x40) == 0;

	if (!apu->fc_irq_enabled)
		cpu_clear_interrupt(&apu->nes->cpu, INT_IRQ);

	if (val & 0x80)
	{
		/* TODO: this actually happens 2 or 3 cycles after the write */
		fc_qframe(apu);
		fc_hframe(apu);
	}
	apu->cycles = 0;
}

static void (*PULSE_REGS[4])(PulseChannel*, uint8_t) = {
	pulse_flags1_write, pulse_sweep_write, pulse_flags3_write, pulse_flags4_write
};

void apu_write(APU* apu, uint16_t addr, uint8_t val)
{
	if (addr >= 0x4000 && addr <= 0x4007)
	{
		/* Writing to a pulse register */
		PulseChannel* channel = (addr & 4) ? &apu->pulse2 : &apu->pulse1;
		PULSE_REGS[addr & 3](channel, val);
		update_sweep_target(apu, channel);
		return;
	}

	/* TODO: implement all registers */
	switch (addr)
	{
		case 0x4008:
			triangle_lin_ctr_write(&apu->triangle, val);
			break;
		case 0x400A:
			triangle_flags2_write(&apu->triangle, val);
			break;
		case 0x400B:
			triangle_flags3_write(&apu->triangle, val);
			break;
		case 0x400C:
			noise_flags1_write(&apu->noise, val);
			break;
		case 0x400E:
			noise_flags2_write(&apu->noise, val);
			break;
		case 0x400F:
			noise_flags3_write(&apu->noise, val);
			break;
		case 0x4015:
			status_write(apu, val);
			break;
		case 0x4017:
			fc_write(apu, val);
			break;
	}
}

uint8_t apu_read(APU* apu, uint16_t addr)
{
	if (addr == 0x4015)
		return status_read(apu);
	else
		return 0;
}

void apu_tick(APU* apu)
{
	uint16_t tri_val = 100 * triangle_clock(&apu->triangle);
	fc_clock(apu);

	if ((apu->cycles % 2) == 0)
	{
		/* TODO: nonlinear mix */
		uint16_t val = 100 * (pulse_clock(&apu->pulse1) + pulse_clock(&apu->pulse2)) + (50 * noise_clock(&apu->noise)) + tri_val;

		/* Output sample */
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
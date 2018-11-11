#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apu.h"
#include "cpu.h"
#include "memory.h"
#include "nes.h"

/* Lookup tables for audio sample non-linear mix */
/*static uint16_t pulse_mix[31];
static uint16_t tnd_mix[203];*/

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

static const uint16_t NOISE_PERIODS[16] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

static const uint8_t DMC_PERIODS[16] = {
	214, 190, 170, 160, 143, 127, 113, 107, 95, 80, 71, 64, 53, 42, 36, 27
};

static void update_irq(APU* apu)
{
	if (apu->fc_irq_fired || apu->dmc.irq_fired)
	{
		cpu_fire_interrupt(&apu->nes->cpu, INT_IRQ);
	}
	else
	{
		cpu_clear_interrupt(&apu->nes->cpu, INT_IRQ);
	}
}

int apu_init(APU* apu, struct NES* nes, NESInitInfo* init_info)
{
	/*uint8_t i;*/
	memset(apu, 0, sizeof(*apu));
	apu->snd_cb = init_info->snd_cb;
	apu->snd_userdata = init_info->snd_userdata;
	apu->nes = nes;

	/* TODO: proper downsampling (a real NES outputs ~1789773 samples/second) */
	/* TODO: adjust buffer size based on host sample rate */
	apu->sample_buf_size = (4096 / sizeof(uint16_t)) * 4;
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

	apu->current_read_buf = apu->sample_buf1;
	apu->current_write_buf = apu->sample_buf2;

	/*pulse_mix[0] = 0;
	for (i = 1; i < 31; ++i)
	{
		float val = 95.52f / ((8128.0f / i) + 100.0f);
		pulse_mix[i] = (uint16_t)(val * 65535);
	}
	tnd_mix[0] = 0;
	for (i = 1; i < 203; ++i)
	{
		float val = 163.67f / ((24329.0f / i) + 100.0f);
		tnd_mix[i] = (uint16_t)(val * 65535);
	}*/
	apu->noise.lfsr = 1;
	return 0;
}


void apu_cleanup(APU* apu)
{
	free(apu->sample_buf1);
	free(apu->sample_buf2);
	memset(apu, 0, sizeof(APU));
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
			update_sweep_target(apu, channel);
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
		env->timer.value = env->timer.period;
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
	env_clock(&apu->noise.env);
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
	if (apu->fc_reset_delay && --apu->fc_reset_delay == 0)
	{
		if (apu->fc_sequence == FC_5STEP)
		{
			fc_qframe(apu);
			fc_hframe(apu);
		}
		apu->cycles = 0;
	}

	/* Steps common to both the 4-step and 5-step sequence */
	switch (apu->cycles)
	{
		case 7457:   /* Step 1 (after 3728.5 APU cycles) */
		case 22371:  /* Step 3 (after 11185.5 APU cycles) */
			fc_qframe(apu);
			break;
		case 14913:  /* Step 2 (after 7456.5 APU cycles) */
			fc_hframe(apu);
			break;
	}
	if (apu->fc_sequence == FC_4STEP)
	{
		switch (apu->cycles)
		{
			case 29829:  /* Second part of step 4 (14914.5 APU cycles) */
				fc_qframe(apu);
				fc_hframe(apu);
				break;
			case 29828:  /* First and third parts of step 4 (14914/14915 APU cycles) */
			case 29830:
				if (apu->fc_irq_enabled)
				{
					apu->fc_irq_fired = 1;
					update_irq(apu);
				}
				apu->cycles %= 29830;
				break;
		}
	}
	else
	{
		switch (apu->cycles)
		{
			case 37281:  /* Step 5 (18640.5 APU cycles) */
				fc_qframe(apu);
				fc_hframe(apu);
				break;
			case 37282:
				apu->cycles = 0;
				break;
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
	if (channel->timer.period < 8 || (!channel->sweep.negate && channel->sweep.target > 0x7FF) || channel->lc.value == 0)
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

	/* Fixes triangle popping during transition from inaudible to audible frequencies.
	   Real hardware does not do this.

	   TODO: remove after implementing a decent audio filter */
	if (channel->timer.period < 2)
		return 0;
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
	if (((channel->lfsr & 1) == 0) && channel->lc.value > 0)
		return channel->env.enabled ? channel->env.decay_vol : channel->env.timer.period;
	return 0;
}

static void dmc_restart_sample(DMCChannel* channel)
{
	channel->curr_addr = channel->sample_addr;
	channel->bytes_remaining = channel->sample_len;
}

/* TODO:
   On the NTSC NES and Famicom, if a new sample byte is fetched from memory at the same time the
   program is reading the controller through $4016/4017, a conflict occurs corrupting the data
   read from the controller. Programs which use DPCM sample playback will normally use a redundant
   controller read routine to work around this defect.

   A similar problem occurs when reading data from the PPU through $2007, or polling $2002 for vblank. */

static uint8_t dmc_clock(APU* apu, DMCChannel* channel)
{
	/* Refill sample buffer if needed */
	if (channel->bytes_remaining > 0 && !channel->sample_buf_filled)
	{
		/* TODO: CPU is stalled to allow the longest possible write */
		apu->nes->cpu.idle_cycles += 4;
		/**/

		channel->sample_buf = memory_get(apu->nes, channel->curr_addr);
		channel->sample_buf_filled = 1;

		/* Wrap curr_addr from $FFFF to $8000 */
		channel->curr_addr = ((channel->curr_addr - 0x8000 + 1) % 0x8000) + 0x8000;
		if (--channel->bytes_remaining == 0)
		{
			 if (channel->loop)
			 {
				 dmc_restart_sample(channel);
			 }
			 else if (channel->irq_enabled)
			 {
				 channel->irq_fired = 1;
				 update_irq(apu);
			 }
		}
	}

	if (channel->timer.value-- == 0)
	{
		if (!channel->silence)
		{
			/* Output must be within [0, 127] */
			uint8_t new_out = channel->output - 2 + ((channel->shift_reg & 1) * 4);
			if ((new_out & 0x80) == 0)
			{
				channel->output = new_out;
			}
			channel->shift_reg >>= 1;
		}

		if (--channel->bits_remaining == 0)
		{
			/* End of output cycle */
			channel->bits_remaining = 8;
			channel->silence = !channel->sample_buf_filled;
			if (!channel->silence)
			{
				channel->shift_reg = channel->sample_buf;
				channel->sample_buf_filled = 0;
			}
		}

		channel->timer.value = channel->timer.period;
	}
	return channel->output;
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
	channel->timer.period = NOISE_PERIODS[val & 0xF];
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
   IL-- RRRR
   ||   ||||
   ||   ++++- Frequency
   |+-------- Loop flag
   +--------- Interrupt enable flag */
static void dmc_flags_write(APU* apu, DMCChannel* channel, uint8_t val)
{
	channel->irq_enabled = (val >> 7) & 1;
	channel->loop = (val >> 6) & 1;
	channel->timer.period = DMC_PERIODS[val & 15];

	if (!channel->irq_enabled)
	{
		channel->irq_fired = 0;
		update_irq(apu);
	}
}

static void dmc_direct_load_write(DMCChannel* channel, uint8_t val)
{
	channel->output = val;
}

static void dmc_sample_addr_write(DMCChannel* channel, uint8_t val)
{
	channel->sample_addr = 0xC000 + (val * 64);
}

static void dmc_sample_len_write(DMCChannel* channel, uint8_t val)
{
	channel->sample_len = (val * 16) + 1;
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

	apu->dmc.irq_fired = 0;
	update_irq(apu);

	if (((val >> 4) & 1) == 0)
	{
		/* Disable DMC */
		apu->dmc.bytes_remaining = 0;
	}
	else if (apu->dmc.bytes_remaining == 0)
	{
		dmc_restart_sample(&apu->dmc);
	}

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
	uint8_t fc_irq = apu->fc_irq_fired;

	apu->fc_irq_fired = 0;
	update_irq(apu);

	return (apu->dmc.irq_fired << 7) |
		   (fc_irq << 6) |
		   ((apu->dmc.bytes_remaining > 0) << 4) |
		   ((apu->noise.lc.value > 0) << 3) |
		   ((apu->triangle.lc.value > 0) << 2) |
		   ((apu->pulse2.lc.value > 0) << 1) |
		   (apu->pulse1.lc.value > 0);
}

/* 7  bit  0
   ---- ----
   MI-- ----
   |+-------- Interrupt inhibit flag
   +--------- Frame counter sequence (0: 4-step, 1: 5-step) */
static void fc_write(APU* apu, uint8_t val)
{
	apu->fc_sequence = (FCSequence)(val >> 7);
	apu->fc_irq_enabled = (val & 0x40) == 0;

	if (!apu->fc_irq_enabled)
	{
		apu->fc_irq_fired = 0;
		update_irq(apu);
	}

	/* If the write occurs _during_ an APU cycle (i.e., odd CPU cycle), the effects occur
	   3 CPU cycles after the $4017 write cycle, and if the write occurs _between_ APU cycles
	   (i.e., even CPU cycle), the effects occurs 4 CPU cycles after the write cycle. */
	apu->fc_reset_delay = 4 - (apu->cycles & 1);
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
		case 0x4010:
			dmc_flags_write(apu, &apu->dmc, val);
			break;
		case 0x4011:
			dmc_direct_load_write(&apu->dmc, val);
			break;
		case 0x4012:
			dmc_sample_addr_write(&apu->dmc, val);
			break;
		case 0x4013:
			dmc_sample_len_write(&apu->dmc, val);
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
	uint16_t tri_val = triangle_clock(&apu->triangle);
	fc_clock(apu);

	if ((apu->cycles % 2) == 0)
	{
		/* Linear approximation of APU non-linear mix.
		   During testing, using the standard lookup table method resulted in static.

		   TODO: try the lookup table method again after implementing DMC and proper
		   downsampling

		   See https://wiki.nesdev.com/w/index.php/APU_Mixer for more info */
		uint16_t pulse_out = 492 * (pulse_clock(&apu->pulse1) + pulse_clock(&apu->pulse2));
		uint16_t tnd_out = (557 * tri_val) + (323 * noise_clock(&apu->noise)) + (219 * dmc_clock(apu, &apu->dmc));
		/*uint16_t pulse_out = pulse_mix[pulse_clock(&apu->pulse1) + pulse_clock(&apu->pulse2)];
		uint16_t tnd_out = tnd_mix[(3 * tri_val) + (2 * noise_clock(&apu->noise))];
		uint16_t val = pulse_out + tnd_out;*/

		/* Output sample */
		if ((apu->cycles % 40) == 0)
		{
			apu->current_write_buf[apu->sample_buf_insert_pos] = pulse_out + tnd_out;
			apu->sample_buf_insert_pos = (apu->sample_buf_insert_pos + 1) % apu->sample_buf_size;
			if (apu->sample_buf_insert_pos == 0)
			{
				uint16_t* tmp = apu->current_write_buf;
				apu->current_write_buf = apu->current_read_buf;
				apu->current_read_buf = tmp;
				//printf("Audio buffer full\n");

				apu->snd_cb(apu->current_read_buf, apu->sample_buf_size, apu->snd_userdata);
			}
		}
	}
	++apu->cycles;
}
#include <stdint.h>
#include <math.h>

#include <SDL.h>

#include "apu.h"



void apu_init(APU* apu)
{
	memset(apu, 0, sizeof(*apu));
}

/* TODO: combine some of these for channels with common properties */
static void pulse_flags1_write(PulseChannel* channel, uint8_t val)
{
	channel->duty = (val >> 6) & 3;
	channel->lc_enabled = (val & 0x20) == 0;
	channel->env_enabled = (val & 0x10) == 0;
	channel->volume = val & 0xF;	
}

static void pulse_flags2_write(PulseChannel* channel, uint8_t val)
{
	/* Write sweep options */
	channel->sweep_enabled = (val & 0x80) != 0;
	channel->sweep_period = (val >> 4) & 7;
	channel->sweep_negate = (val & 8) != 0;
	channel->sweep_shift = val & 7;
}

static void pulse_flags3_write(PulseChannel* channel, uint8_t val)
{
	/* Write timer lo */
	channel->timer = (channel->timer & 0x700) | val;
}

static void pulse_flags4_write(PulseChannel* channel, uint8_t val)
{
	/* Write timer hi and length counter reload index */
	/* TODO: also resets duty and starts envelope */
	channel->timer = (channel->timer & 0xFF) | ((val & 7) << 8);
	channel->lc_load = (val >> 3) & 0x1F;  /* TODO: reload directly? */
}

static void triangle_flags1_write(TriangleChannel* channel, uint8_t val)
{
	channel->lc_enabled = (val & 0x80) == 0;
	channel->lin_counter_reload = val & 0x7F;
}

static void triangle_flags2_write(TriangleChannel* channel, uint8_t val)
{
	/* Write timer lo */
	channel->timer = (channel->timer & 0x700) | val;
}

static void triangle_flags3_write(TriangleChannel* channel, uint8_t val)
{
	/* Write timer hi and length counter reload index */
	/* TODO: also resets duty and starts envelope */
	channel->timer = (channel->timer & 0xFF) | ((val & 7) << 8);
	channel->lc_load = (val >> 3) & 0x1F;  /* TODO: reload directly? */
}

static void noise_flags1_write(NoiseChannel* channel, uint8_t val)
{
	channel->lc_enabled = (val & 0x20) == 0;
	channel->env_enabled = (val & 0x10) == 0;
	channel->volume = val & 0xF;
}

static void noise_flags2_write(NoiseChannel* channel, uint8_t val)
{
	channel->loop_enabled = (val & 0x80) != 0;
	channel->period = val & 0xF;
}

static void noise_flags3_write(NoiseChannel* channel, uint8_t val)
{
	/* TODO: also starts envelope */
	channel->lc_load = (val >> 3) & 0x1F;  /* TODO: reload directly? */
}

static void dmc_flags1_write(DMCChannel* channel, uint8_t val)
{
	channel->irq_enabled = (val & 0x80) != 0;
	channel->loop_enabled = (val & 0x40) != 0;
	channel->freq_idx = val & 0xF;
}

static void dmc_flags2_write(DMCChannel* channel, uint8_t val)
{
	channel->direct_load = val & 0x7F;
}

static void dmc_flags3_write(DMCChannel* channel, uint8_t val)
{
	/* TODO: don't think this is right (%11AAAAAA.AA000000) */
	channel->sample_addr = val;
}

static void dmc_flags4_write(DMCChannel* channel, uint8_t val)
{
	/* TODO: don't think this is right (%0000LLLL.LLLL0001) */
	channel->sample_len = val;
}

static void apuctrl_write(APU* apu, uint8_t val)
{
	/* TODO:
	     Writing a zero to any of the channel enable bits will silence that channel and immediately set its length counter to 0.
		If the DMC bit is clear, the DMC bytes remaining will be set to 0 and the DMC will silence when it empties.
		If the DMC bit is set, the DMC sample will be restarted only if its bytes remaining is 0. If there are bits remaining in the 1-byte sample buffer, these will finish playing before the next sample is fetched.
		Writing to this register clears the DMC interrupt flag.
		Power-up and reset have the effect of writing $00, silencing all channels. */
}

static uint8_t apustatus_read(APU* apu)
{
	/* TODO:
	     N/T/2/1 will read as 1 if the corresponding length counter is greater than 0. For the triangle channel, the status of the linear counter is irrelevant.
		D will read as 1 if the DMC bytes remaining is more than 0.
		Reading this register clears the frame interrupt flag (but not the DMC interrupt flag).
		If an interrupt flag was set at the same moment of the read, it will read back as 1 but it will not be cleared. */
	return 0;
}

static void apuframecnt_write(APU* apu, uint8_t val)
{
	apu->fc_seq_mode = (val & 0x80) != 0;
	/* TODO:
		Interrupt inhibit flag. If set, the frame interrupt flag is cleared, otherwise it is unaffected. 
		After 3 or 4 CPU clock cycles*, the timer is reset.
		If the mode flag is set, then both "quarter frame" and "half frame" signals are also generated.  */
}

void apu_write(APU* apu, uint16_t addr, uint8_t val)
{
	/* TODO: Implement this (with open bus when reading from write-only registers) */
	/* TODO: efficiently determine which channel's registers are being written to */
	switch (addr)
	{
		case 0x4000:
			pulse_flags1_write(&apu->pulse1, val);
			break;
		case 0x4001:
			pulse_flags2_write(&apu->pulse1, val);
			break;
		case 0x4002:
			pulse_flags3_write(&apu->pulse1, val);
			break;
		case 0x4003:
			pulse_flags4_write(&apu->pulse1, val);
			break;
		case 0x4004:
			pulse_flags1_write(&apu->pulse2, val);
			break;
		case 0x4005:
			pulse_flags2_write(&apu->pulse2, val);
			break;
		case 0x4006:
			pulse_flags3_write(&apu->pulse2, val);
			break;
		case 0x4007:
			pulse_flags4_write(&apu->pulse2, val);
			break;
		case 0x4008:
			triangle_flags1_write(&apu->triangle, val);
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
			dmc_flags1_write(&apu->dmc, val);
			break;
		case 0x4011:
			dmc_flags2_write(&apu->dmc, val);
			break;
		case 0x4012:
			dmc_flags3_write(&apu->dmc, val);
			break;
		case 0x4013:
			dmc_flags4_write(&apu->dmc, val);
			break;
		case 0x4015:
			apuctrl_write(apu, val);
			break;
		case 0x4017:
			apuframecnt_write(apu, val);
			break;
	}
}

uint8_t apu_read (APU* apu, uint16_t addr)
{
	/* TODO: Implement this (with open bus when reading from write-only registers) */
	/* TODO: efficiently determine which channel's registers are being written to */
	if (addr == 0x4015)
	{
		return apustatus_read(apu);
	}
}
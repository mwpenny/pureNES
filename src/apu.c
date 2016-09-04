#include <stdint.h>
#include <math.h>

#include <SDL.h>

#include "nes.h"
#include "cpu.h"
#include "apu.h"

#ifndef M_TAU
#define M_TAU 6.28318530717958647693
#endif

static uint8_t pulse_cycles[4] = {
	0x40,  /* 01000000 - 12.5% duty */
	0x60,  /* 01100000 - 25% duty   */
	0x78,  /* 01111000 - 50% duty   */
	0x9F   /* 10011111 - 75% duty   */
};

static void audio_callback(void* userdata, uint8_t* stream, int len)
{
	APU* apu = (APU*)userdata;

	int i = 0;
    while (i < len)
	{
		double f1 = (19687500/11)/(32*(apu->pulse1.timer.period+1));
		double f2 = (19687500/11)/(32*(apu->pulse2.timer.period+1));

		/* TODO: use duty cycle */
		uint8_t wave = (pulse_cycles[apu->pulse1.duty] >> (7 - apu->pulse1.phase)) & 1;

		/* TODO: don't hard code this */
		static double phase1, phase2;

		stream[i] = apu->pulse1.volume * sin(phase1);
		stream[i] += apu->pulse2.volume * sin(phase2);

		phase1 += (f1 * M_TAU / apu->sample_rate);
		phase2 += (f2 * M_TAU / apu->sample_rate);
        ++i;
    }
}

void apu_init(APU* apu, NES* nes)
{
	SDL_AudioSpec desired, obtained;
	memset(apu, 0, sizeof(*apu));
	apu->sample_rate = 44100;
	apu->nes = nes;

	/* TODO: not exact? "The NES outputs ~1789773 samples a second whereas PCs
	   typically only do 44100 (assuming 44KHz output). You essentially have to
	   combine the output for several cycles into one sample */
    desired.freq = apu->sample_rate;
    desired.format = AUDIO_S16SYS;  /* 32 bit support in SDL 2 */
    desired.channels = 1;
    desired.samples = 2048; /* TODO: adjust? */
    desired.callback = audio_callback;
	desired.userdata = apu;

    /* TODO: error checking.
	   Returns 0 if successful, placing actual hardware params in obtained.
	   If obtained is NULL, audio data passed to callback function will be
	   guaranteed to be in the requested format, and will be automatically
	   converted to hardware audio format if necessary. Returns -1 if it
	   failed to open the audio device, or couldn't set up the audio thread. */
    SDL_OpenAudio(&desired, &obtained);

    /* Enable audio device.

	TODO: Since audio driver may modify requested size of audio buffer,
	should allocate any local mixing buffers after device is opened. */
    SDL_PauseAudio(0);
}

void apu_cleanup(APU* apu)
{
	SDL_CloseAudio();
}

static void pulse_step(PulseChannel* channel)
{
	if (channel->timer.value-- == 0)
	{
		channel->timer.value = channel->timer.period;
		channel->phase = (channel->phase + 1) & 7;
	}
}

static void clock_quarter_frame(APU* apu)
{
	/* Clock envelopes and triangle linear counter */
}

static void clock_half_frame(APU* apu)
{
	/* Clock length counters and sweep units */
}

static uint16_t seq_step_cycles[2][5] = {
	{7457, 14913, 22371, 29828},
	{7457, 14913, 22371, 29829, 37281}
};

void apu_tick(APU* apu)
{
	if (apu->cycle == seq_step_cycles[apu->seq_mode][apu->seq_step])
	{
		/* Frame sequencer stages
		   4-step     5-step       Action
		   --------------------------------------
		   - - - f    - - - - -    Fire interrupt
		   - l - l    - l - - l    Clock sweep
		   e e e e    e e e - e    Clock decay */
		if (apu->seq_step < 3 || apu->seq_step == apu->seq_mode + 3)
			clock_quarter_frame(apu);
		if (apu->seq_step == 1 || apu->seq_step == apu->seq_mode + 3)
			clock_half_frame(apu);
		if (apu->seq_mode == 0 && apu->seq_step == 3 && apu->irq_enabled)
			cpu_interrupt(&apu->nes->cpu, INT_IRQ);

		apu->seq_step = apu->seq_step % (apu->seq_mode + 4);
	}

	/* APU runs at half speed of CPU */
	if (apu->cycle % 2)
	{
		pulse_step(&apu->pulse1);
		pulse_step(&apu->pulse2);
	}

	/* TODO: deal with overflow */
	++apu->cycle;
}

/* TODO: combine some of these for channels with common properties */
static void pulse_flags1_write(PulseChannel* channel, uint8_t val)
{
	channel->duty = pulse_cycles[(val >> 6) & 3];
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
	channel->timer.period = (channel->timer.period & 0x700) | val;
}

static void pulse_flags4_write(PulseChannel* channel, uint8_t val)
{
	/* Write timer hi and length counter reload index */
	/* TODO: also resets duty and starts envelope */
	channel->timer.period = (channel->timer.period & 0xFF) | ((val & 7) << 8);
	channel->lc_load = (val >> 3) & 0x1F;  /* TODO: reload directly? */

	channel->phase = 0;
}

static void triangle_flags1_write(TriangleChannel* channel, uint8_t val)
{
	channel->lc_enabled = (val & 0x80) == 0;
	channel->lin_counter_reload = val & 0x7F;
}

static void triangle_flags2_write(TriangleChannel* channel, uint8_t val)
{
	/* Write timer lo */
	channel->timer.period = (channel->timer.period & 0x700) | val;
}

static void triangle_flags3_write(TriangleChannel* channel, uint8_t val)
{
	/* Write timer hi and length counter reload index */
	/* TODO: also resets duty and starts envelope */
	channel->timer.period = (channel->timer.period & 0xFF) | ((val & 7) << 8);
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
	apu->seq_mode = (val >> 7) & 1;
	apu->irq_enabled = (val & 0x40) == 0;
	/* TODO:
		Interrupt inhibit flag. If set, the frame interrupt flag is cleared, otherwise it is unaffected. 
		After 3 or 4 CPU clock cycles*, the timer is reset.
		If the mode flag is set, then both "quarter frame" and "half frame" signals are also generated.  */

	/* TODO: If the write occurs during an APU cycle, the effects occur 3 CPU cycles after the $4017 write
	   cycle, and if the write occurs between APU cycles, the effects occurs 4 CPU cycles after the write cycle. */
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
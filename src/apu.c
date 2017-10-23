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

/* 7  bit  0
   ---- ----
   DDLC VVVV
   |||| ||||
   |||| ++++- Volume/envelope (toggled by bit 5)
   |||+------ Controls whether lower 4 bits are constant volume or
   |||        envelope period
   ||+------- Length counter halt (when set, pauses/silences channel)
   ++-------- Duty cycle select */
static void pulse_flags1_write(PulseChannel* channel, uint8_t val)
{
	channel->duty_cycle = PULSE_DUTY_CYCLES[(val >> 6) & 3];
	channel->lc_halted = (val >> 5) & 1;
	channel->using_const_vol = (val >> 4) & 1;
	channel->vol_env = val & 0xF;
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
	channel->length_counter = LC_INIT_VALUES[(val >> 3) & 0x1F];
	//channel->phase = 0;
}

static uint8_t pulse_tick(PulseChannel* channel)
{
	/* Clock pulse waveform generator */
	if (channel->timer.value-- == 0)
	{
		channel->timer.value = channel->timer.period;
		channel->phase = (channel->phase + 1) & 7;
	}
	return channel->vol_env * ((channel->duty_cycle >> (7 - channel->phase)) & 1);
}

void apu_write(APU* apu, uint16_t addr, uint8_t val)
{
	/* TODO: implement all registers */
	/* TODO: optimize (look up tables may be useful for units with the same
	   types of registers (i.e., pulse) */
	switch (addr)
	{
		case 0x4000:
			pulse_flags1_write(&apu->pulse1, val);
			break;
		/*case 0x4001:
			pulse_flags2_write(&apu->pulse1, val);
			break;*/
		case 0x4002:
			pulse_flags3_write(&apu->pulse1, val);
			break;
		case 0x4003:
			pulse_flags4_write(&apu->pulse1, val);
			break;
		case 0x4004:
			pulse_flags1_write(&apu->pulse2, val);
			break;
		/*case 0x4005:
			pulse_flags2_write(&apu->pulse2, val);
			break;*/
		case 0x4006:
			pulse_flags3_write(&apu->pulse2, val);
			break;
		case 0x4007:
			pulse_flags4_write(&apu->pulse2, val);
			break;
	}
}

uint8_t apu_read(APU* apu, uint16_t addr)
{
	/* TODO */
	return 0;
}

void apu_tick(APU* apu)
{
	/* TODO: safe threading */
	uint16_t val = 100 * (pulse_tick(&apu->pulse1) + pulse_tick(&apu->pulse2));

	if ((apu->cycle++ % 20) == 0)
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

	/*
		1) Clock main divider (~240Hz)
			-Clocks sequencer (controls length counters, sweep units, the
			 unit's linear counter, and generates IRQs) */
}
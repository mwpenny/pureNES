/* Ricoh 2A03 (MOH 6502) */

#ifndef CPU_H
#define CPU_H

#include <stdint.h>

typedef enum {
	INT_NONE,
	INT_RESET,
	INT_NMI,
	INT_IRQ
} InterruptType;

typedef enum {
	AMODE_ACC,
	AMODE_IMP,
	AMODE_IMM,
	AMODE_ZPG,
	AMODE_ZPX,
	AMODE_ZPY,
	AMODE_REL,
	AMODE_ABS,
	AMODE_ABX,
	AMODE_ABY,
	AMODE_IND,
	AMODE_XID,
	AMODE_IDY
} AddressingMode;

typedef struct
{
	struct NES* nes;

	uint16_t pc;
	uint8_t sp;
	uint8_t p;
	uint8_t a, x, y;

	uint8_t is_running;
	InterruptType last_int;
	uint8_t opcode;
	AddressingMode instr_amode;
	uint16_t eff_addr;

	int cycles, dma_cycles;
	uint8_t oddcycle;
} CPU;

void cpu_init(CPU* cpu, struct NES* nes);
void cpu_power(CPU* cpu);
void cpu_reset(CPU* cpu);
int cpu_step(CPU* cpu);
void cpu_interrupt(CPU* cpu, uint8_t type);

#endif
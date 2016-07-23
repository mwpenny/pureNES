/* Ricoh 2A03 (MOH 6502) */

#ifndef CPU_H
#define CPU_H

#include <stdint.h>

typedef enum {
	INT_NONE,
	INT_RESET,
	INT_NMI,
	INT_IRQ
} Interrupt;

typedef enum {
	AMODE_NONE,
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
	struct NES* nes;	/* the system the CPU is connected to */

	Interrupt interrupt;	/* last interrupt received */
	uint16_t pc;			/* program counter */
	uint8_t sp;				/* stack pointer */
	uint8_t p;				/* processor flags */
	uint8_t a, x, y;		/* other registers */

	uint8_t opcode;
	AddressingMode instruction_amode;
	uint16_t oc_addr;

	int cycles, dma_cycles;
	uint8_t oddcycle;
} CPU;

/*typedef struct
{
	AddressingMode amode;
	uint8_t opcode;
	uint16_t operand;
} OCInfo;*/

void cpu_init(CPU* cpu, struct NES* nes);
void cpu_reset(CPU* cpu);
int cpu_step(CPU* cpu);
void cpu_interrupt(CPU* cpu, uint8_t type);

#endif
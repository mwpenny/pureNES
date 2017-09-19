#ifndef CPU_H
#define CPU_H

#include <stdint.h>

typedef enum {
	INT_RST = 1,
	INT_NMI = 2,
	INT_IRQ = 3
} Interrupt;

typedef enum {
	AMODE_ACC,  /* Accumulator addressing */
	AMODE_IMP,  /* Implied addressing */
	AMODE_IMM,  /* Immediate addressing */
	AMODE_ZPG,  /* Zero page addressing */
	AMODE_ZPX,  /* Zero page X-indexed addressing */
	AMODE_ZPY,  /* Zero page Y-indexed addressing */
	AMODE_REL,  /* Relative addressing */
	AMODE_ABS,  /* Absolute addressing */
	AMODE_ABX,  /* Absolute X-indexed addressing */
	AMODE_ABY,  /* Absolute Y-indexed addressing */
	AMODE_IND,  /* Indirect addressing */
	AMODE_XID,  /* Indexed indirect addressing */
	AMODE_IDY   /* Indirect indexed addressing */
} AddressingMode;

typedef struct
{
	struct NES* nes;

	/* Registers */
	uint16_t pc;
	uint8_t sp;
	uint8_t p;
	uint8_t a, x, y;

	uint8_t is_running;/*, oam_dma_started;
	uint8_t oam_dma_bytes_copied;*/
	uint8_t opcode;
	uint8_t pending_interrupts;
	AddressingMode instr_amode;
	uint16_t eff_addr/*, oam_dma_addr*/;
	uint16_t cycles, idle_cycles;
} CPU;

void cpu_init(CPU* cpu, struct NES* nes);
void cpu_power(CPU* cpu);
void cpu_reset(CPU* cpu);
uint16_t cpu_step(CPU* cpu);
void cpu_interrupt(CPU* cpu, Interrupt type);
/*oid cpu_begin_oam_dma(CPU* cpu, uint16_t addr_start);*/

#endif

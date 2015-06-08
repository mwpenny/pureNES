/* Ricoh 2A03 (MOH 6502) */

#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdio.h>
#include "memory.h"

#define ADDR_RESET 0xFFFC
#define ADDR_BRK 0xFFFE

#define MASK_C 0x01
#define MASK_Z 0x02
#define MASK_I 0x04
#define MASK_D 0x08
#define MASK_B 0x10
#define MASK_V 0x40
#define MASK_N 0x80

typedef struct
{
	uint16_t pc;		/* program counter */ /* int may be faster */
	uint8_t sp;			/* stack pointer */
	uint8_t p;			/* processor flags */
	uint8_t a, x, y;	/* registers */
} CPU;

typedef struct
{
	uint8_t opcode;
	uint16_t operand;
} OCInfo;

void cpu_tick(CPU* cpu, FILE* log);
void cpu_reset(CPU* cpu);

/* Addressing mode handlers */
void amode_NUL(CPU* cpu, OCInfo* oci);
void amode_IMM(CPU* cpu, OCInfo* oci);
void amode_ZPG(CPU* cpu, OCInfo* oci);
void amode_ZPX(CPU* cpu, OCInfo* oci);
void amode_ZPY(CPU* cpu, OCInfo* oci);
void amode_REL(CPU* cpu, OCInfo* oci);
void amode_ABS(CPU* cpu, OCInfo* oci);
void amode_ABX(CPU* cpu, OCInfo* oci);
void amode_ABY(CPU* cpu, OCInfo* oci);
void amode_IND(CPU* cpu, OCInfo* oci);
void amode_XID(CPU* cpu, OCInfo* oci);
void amode_IDY(CPU* cpu, OCInfo* oci);

/*** TODO: cycle counting ***/
/*** TODO: make static/inline? ***/

/* Load/store operations */
void cpu_LDA(CPU* cpu, OCInfo* oci);
void cpu_LDX(CPU* cpu, OCInfo* oci);
void cpu_LDY(CPU* cpu, OCInfo* oci);
void cpu_STA(CPU* cpu, OCInfo* oci);
void cpu_STX(CPU* cpu, OCInfo* oci);
void cpu_STY(CPU* cpu, OCInfo* oci);

/* Register transfers */
void cpu_TAX(CPU* cpu, OCInfo* oci);
void cpu_TAY(CPU* cpu, OCInfo* oci);
void cpu_TXA(CPU* cpu, OCInfo* oci);
void cpu_TYA(CPU* cpu, OCInfo* oci);

/* Stack operations */
void cpu_TSX(CPU* cpu, OCInfo* oci);
void cpu_TXS(CPU* cpu, OCInfo* oci);
void cpu_PHA(CPU* cpu, OCInfo* oci);
void cpu_PHA(CPU* cpu, OCInfo* oci);
void cpu_PHP(CPU* cpu, OCInfo* oci);
void cpu_PLA(CPU* cpu, OCInfo* oci);
void cpu_PLP(CPU* cpu, OCInfo* oci);

/* Logical operations */
void cpu_AND(CPU* cpu, OCInfo* oci);
void cpu_EOR(CPU* cpu, OCInfo* oci);
void cpu_ORA(CPU* cpu, OCInfo* oci);
void cpu_BIT(CPU* cpu, OCInfo* oci);

/* Arithmetic operations */
void cpu_ADC(CPU* cpu, OCInfo* oci);
void cpu_SBC(CPU* cpu, OCInfo* oci);
void cpu_CMP(CPU* cpu, OCInfo* oci);
void cpu_CPX(CPU* cpu, OCInfo* oci);
void cpu_CPY(CPU* cpu, OCInfo* oci);

/* Increments and decrements */
void cpu_INC(CPU* cpu, OCInfo* oci);
void cpu_INX(CPU* cpu, OCInfo* oci);
void cpu_INY(CPU* cpu, OCInfo* oci);
void cpu_DEC(CPU* cpu, OCInfo* oci);
void cpu_DEX(CPU* cpu, OCInfo* oci);
void cpu_DEY(CPU* cpu, OCInfo* oci);

/* Shifts */
void cpu_ASL(CPU* cpu, OCInfo* oci);
void cpu_LSR(CPU* cpu, OCInfo* oci);
void cpu_ROL(CPU* cpu, OCInfo* oci);
void cpu_ROR(CPU* cpu, OCInfo* oci);

/* Jumps and calls */
void cpu_JMP(CPU* cpu, OCInfo* oci);
void cpu_JSR(CPU* cpu, OCInfo* oci);
void cpu_RTS(CPU* cpu, OCInfo* oci);

/* Conditional branches */
void cpu_BCC(CPU* cpu, OCInfo* oci);
void cpu_BCS(CPU* cpu, OCInfo* oci);
void cpu_BEQ(CPU* cpu, OCInfo* oci);
void cpu_BMI(CPU* cpu, OCInfo* oci);
void cpu_BNE(CPU* cpu, OCInfo* oci);
void cpu_BPL(CPU* cpu, OCInfo* oci);
void cpu_BVC(CPU* cpu, OCInfo* oci);
void cpu_BVS(CPU* cpu, OCInfo* oci);

/* Status flag changes */
void cpu_CLC(CPU* cpu, OCInfo* oci);
void cpu_CLD(CPU* cpu, OCInfo* oci);
void cpu_CLI(CPU* cpu, OCInfo* oci);
void cpu_CLV(CPU* cpu, OCInfo* oci);
void cpu_SEC(CPU* cpu, OCInfo* oci);
void cpu_SED(CPU* cpu, OCInfo* oci);
void cpu_SEI(CPU* cpu, OCInfo* oci);

/* System */
void cpu_BRK(CPU* cpu, OCInfo* oci); /*TODO: brk apparently (sort of) HAS AN OPERAND (second byte is padding)*/
void cpu_NOP(CPU* cpu, OCInfo* oci);
void cpu_RTI(CPU* cpu, OCInfo* oci);

/* TODO: Undocumented instructions */
void cpu_KIL(CPU* cpu, OCInfo* oci);

/* Lookup tables */
static void (*opcodes[256])(CPU* cpu, OCInfo* oci) = 
{
	cpu_BRK, cpu_ORA, cpu_KIL, cpu_NOP, cpu_NOP, cpu_ORA, cpu_ASL, cpu_NOP, 
	cpu_PHP, cpu_ORA, cpu_ASL, cpu_NOP, cpu_NOP, cpu_ORA, cpu_ASL, cpu_NOP, 
	cpu_BPL, cpu_ORA, cpu_KIL, cpu_NOP, cpu_NOP, cpu_ORA, cpu_ASL, cpu_NOP, 
	cpu_CLC, cpu_ORA, cpu_NOP, cpu_NOP, cpu_NOP, cpu_ORA, cpu_ASL, cpu_NOP, 
	cpu_JSR, cpu_AND, cpu_KIL, cpu_NOP, cpu_BIT, cpu_AND, cpu_ROL, cpu_NOP, 
	cpu_PLP, cpu_AND, cpu_ROL, cpu_NOP, cpu_BIT, cpu_AND, cpu_ROL, cpu_NOP, 
	cpu_BMI, cpu_AND, cpu_KIL, cpu_NOP, cpu_NOP, cpu_AND, cpu_ROL, cpu_NOP, 
	cpu_SEC, cpu_AND, cpu_NOP, cpu_NOP, cpu_NOP, cpu_AND, cpu_ROL, cpu_NOP, 
	cpu_RTI, cpu_EOR, cpu_KIL, cpu_NOP, cpu_NOP, cpu_EOR, cpu_LSR, cpu_NOP, 
	cpu_PHA, cpu_EOR, cpu_LSR, cpu_NOP, cpu_JMP, cpu_EOR, cpu_LSR, cpu_NOP, 
	cpu_BVC, cpu_EOR, cpu_KIL, cpu_NOP, cpu_NOP, cpu_EOR, cpu_LSR, cpu_NOP, 
	cpu_CLI, cpu_EOR, cpu_NOP, cpu_NOP, cpu_NOP, cpu_EOR, cpu_LSR, cpu_NOP, 
	cpu_RTS, cpu_ADC, cpu_KIL, cpu_NOP, cpu_NOP, cpu_ADC, cpu_ROR, cpu_NOP, 
	cpu_PLA, cpu_ADC, cpu_ROR, cpu_NOP, cpu_JMP, cpu_ADC, cpu_ROR, cpu_NOP, 
	cpu_BVS, cpu_ADC, cpu_KIL, cpu_NOP, cpu_NOP, cpu_ADC, cpu_ROR, cpu_NOP, 
	cpu_SEI, cpu_ADC, cpu_NOP, cpu_NOP, cpu_NOP, cpu_ADC, cpu_ROR, cpu_NOP, 
	cpu_NOP, cpu_STA, cpu_NOP, cpu_NOP, cpu_STY, cpu_STA, cpu_STX, cpu_NOP, 
	cpu_DEY, cpu_NOP, cpu_TXA, cpu_NOP, cpu_STY, cpu_STA, cpu_STX, cpu_NOP, 
	cpu_BCC, cpu_STA, cpu_KIL, cpu_NOP, cpu_STY, cpu_STA, cpu_STX, cpu_NOP, 
	cpu_TYA, cpu_STA, cpu_TXS, cpu_NOP, cpu_NOP, cpu_STA, cpu_NOP, cpu_NOP, 
	cpu_LDY, cpu_LDA, cpu_LDX, cpu_NOP, cpu_LDY, cpu_LDA, cpu_LDX, cpu_NOP, 
	cpu_TAY, cpu_LDA, cpu_TAX, cpu_NOP, cpu_LDY, cpu_LDA, cpu_LDX, cpu_NOP, 
	cpu_BCS, cpu_LDA, cpu_KIL, cpu_NOP, cpu_LDY, cpu_LDA, cpu_LDX, cpu_NOP, 
	cpu_CLV, cpu_LDA, cpu_TSX, cpu_NOP, cpu_LDY, cpu_LDA, cpu_LDX, cpu_NOP, 
	cpu_CPY, cpu_CMP, cpu_NOP, cpu_NOP, cpu_CPY, cpu_CMP, cpu_DEC, cpu_NOP, 
	cpu_INY, cpu_CMP, cpu_DEX, cpu_NOP, cpu_CPY, cpu_CMP, cpu_DEC, cpu_NOP, 
	cpu_BNE, cpu_CMP, cpu_KIL, cpu_NOP, cpu_NOP, cpu_CMP, cpu_DEC, cpu_NOP, 
	cpu_CLD, cpu_CMP, cpu_NOP, cpu_NOP, cpu_NOP, cpu_CMP, cpu_DEC, cpu_NOP, 
	cpu_CPX, cpu_SBC, cpu_NOP, cpu_NOP, cpu_CPX, cpu_SBC, cpu_INC, cpu_NOP, 
	cpu_INX, cpu_SBC, cpu_NOP, cpu_NOP, cpu_CPX, cpu_SBC, cpu_INC, cpu_NOP, 
	cpu_BEQ, cpu_SBC, cpu_KIL, cpu_NOP, cpu_NOP, cpu_SBC, cpu_INC, cpu_NOP, 
	cpu_SED, cpu_SBC, cpu_NOP, cpu_NOP, cpu_NOP, cpu_SBC, cpu_INC, cpu_NOP
};

static char* oc_names[256] = 
{
	"BRK", "ORA", "KIL", "NOP", "NOP", "ORA", "ASL", "NOP",
	"PHP", "ORA", "ASL", "NOP", "NOP", "ORA", "ASL", "NOP",
	"BPL", "ORA", "KIL", "NOP", "NOP", "ORA", "ASL", "NOP",
	"CLC", "ORA", "NOP", "NOP", "NOP", "ORA", "ASL", "NOP",
	"JSR", "AND", "KIL", "NOP", "BIT", "AND", "ROL", "NOP",
	"PLP", "AND", "ROL", "NOP", "BIT", "AND", "ROL", "NOP",
	"BMI", "AND", "KIL", "NOP", "NOP", "AND", "ROL", "NOP",
	"SEC", "AND", "NOP", "NOP", "NOP", "AND", "ROL", "NOP",
	"RTI", "EOR", "KIL", "NOP", "NOP", "EOR", "LSR", "NOP",
	"PHA", "EOR", "LSR", "NOP", "JMP", "EOR", "LSR", "NOP",
	"BVC", "EOR", "KIL", "NOP", "NOP", "EOR", "LSR", "NOP",
	"CLI", "EOR", "NOP", "NOP", "NOP", "EOR", "LSR", "NOP",
	"RTS", "ADC", "KIL", "NOP", "NOP", "ADC", "ROR", "NOP",
	"PLA", "ADC", "ROR", "NOP", "JMP", "ADC", "ROR", "NOP",
	"BVS", "ADC", "KIL", "NOP", "NOP", "ADC", "ROR", "NOP",
	"SEI", "ADC", "NOP", "NOP", "NOP", "ADC", "ROR", "NOP",
	"NOP", "STA", "NOP", "NOP", "STY", "STA", "STX", "NOP",
	"DEY", "NOP", "TXA", "NOP", "STY", "STA", "STX", "NOP",
	"BCC", "STA", "KIL", "NOP", "STY", "STA", "STX", "NOP",
	"TYA", "STA", "TXS", "NOP", "NOP", "STA", "NOP", "NOP",
	"LDY", "LDA", "LDX", "NOP", "LDY", "LDA", "LDX", "NOP",
	"TAY", "LDA", "TAX", "NOP", "LDY", "LDA", "LDX", "NOP",
	"BCS", "LDA", "KIL", "NOP", "LDY", "LDA", "LDX", "NOP",
	"CLV", "LDA", "TSX", "NOP", "LDY", "LDA", "LDX", "NOP",
	"CPY", "CMP", "NOP", "NOP", "CPY", "CMP", "DEC", "NOP",
	"INY", "CMP", "DEX", "NOP", "CPY", "CMP", "DEC", "NOP",
	"BNE", "CMP", "KIL", "NOP", "NOP", "CMP", "DEC", "NOP",
	"CLD", "CMP", "NOP", "NOP", "NOP", "CMP", "DEC", "NOP",
	"CPX", "SBC", "NOP", "NOP", "CPX", "SBC", "INC", "NOP",
	"INX", "SBC", "NOP", "NOP", "CPX", "SBC", "INC", "NOP",
	"BEQ", "SBC", "KIL", "NOP", "NOP", "SBC", "INC", "NOP",
	"SED", "SBC", "NOP", "NOP", "NOP", "SBC", "INC", "NOP"
};

static void (*amodes[256])(CPU* cpu, OCInfo* oci) = 
{
	amode_NUL, amode_XID, amode_NUL, amode_NUL, amode_NUL, amode_ZPG, amode_ZPG, amode_NUL,
	amode_NUL, amode_IMM, amode_NUL, amode_NUL, amode_NUL, amode_ABS, amode_ABS, amode_NUL,
	amode_REL, amode_IDY, amode_NUL, amode_NUL, amode_NUL, amode_ZPX, amode_ZPX, amode_NUL,
	amode_NUL, amode_ABY, amode_NUL, amode_NUL, amode_NUL, amode_ABX, amode_ABX, amode_NUL,
	amode_ABS, amode_XID, amode_NUL, amode_NUL, amode_ZPG, amode_ZPG, amode_ZPG, amode_NUL,
	amode_NUL, amode_IMM, amode_NUL, amode_NUL, amode_ABS, amode_ABS, amode_ABS, amode_NUL,
	amode_REL, amode_IDY, amode_NUL, amode_NUL, amode_NUL, amode_ZPX, amode_ZPX, amode_NUL,
	amode_NUL, amode_ABY, amode_NUL, amode_NUL, amode_NUL, amode_ABX, amode_ABX, amode_NUL,
	amode_NUL, amode_XID, amode_NUL, amode_NUL, amode_NUL, amode_ZPG, amode_ZPG, amode_NUL,
	amode_NUL, amode_IMM, amode_NUL, amode_NUL, amode_ABS, amode_ABS, amode_ABS, amode_NUL,
	amode_REL, amode_IDY, amode_NUL, amode_NUL, amode_NUL, amode_ZPX, amode_ZPX, amode_NUL,
	amode_NUL, amode_ABY, amode_NUL, amode_NUL, amode_NUL, amode_ABX, amode_ABX, amode_NUL,
	amode_NUL, amode_XID, amode_NUL, amode_NUL, amode_NUL, amode_ZPG, amode_ZPG, amode_NUL,
	amode_NUL, amode_IMM, amode_NUL, amode_NUL, amode_IND, amode_ABS, amode_ABS, amode_NUL,
	amode_REL, amode_IDY, amode_NUL, amode_NUL, amode_NUL, amode_ZPX, amode_ZPX, amode_NUL,
	amode_NUL, amode_ABY, amode_NUL, amode_NUL, amode_NUL, amode_ABX, amode_ABX, amode_NUL,
	amode_NUL, amode_XID, amode_NUL, amode_NUL, amode_ZPG, amode_ZPG, amode_ZPG, amode_NUL,
	amode_NUL, amode_NUL, amode_NUL, amode_NUL, amode_ABS, amode_ABS, amode_ABS, amode_NUL,
	amode_REL, amode_IDY, amode_NUL, amode_NUL, amode_ZPX, amode_ZPX, amode_ZPY, amode_NUL,
	amode_NUL, amode_ABY, amode_NUL, amode_NUL, amode_NUL, amode_ABX, amode_NUL, amode_NUL,
	amode_IMM, amode_XID, amode_IMM, amode_NUL, amode_ZPG, amode_ZPG, amode_ZPG, amode_NUL,
	amode_NUL, amode_IMM, amode_NUL, amode_NUL, amode_ABS, amode_ABS, amode_ABS, amode_NUL,
	amode_REL, amode_IDY, amode_NUL, amode_NUL, amode_ZPX, amode_ZPX, amode_ZPY, amode_NUL,
	amode_NUL, amode_ABY, amode_NUL, amode_NUL, amode_ABX, amode_ABX, amode_ABY, amode_NUL,
	amode_IMM, amode_XID, amode_NUL, amode_NUL, amode_ZPG, amode_ZPG, amode_ZPG, amode_NUL,
	amode_NUL, amode_IMM, amode_NUL, amode_NUL, amode_ABS, amode_ABS, amode_ABS, amode_NUL,
	amode_REL, amode_IDY, amode_NUL, amode_NUL, amode_NUL, amode_ZPX, amode_ZPX, amode_NUL,
	amode_NUL, amode_ABY, amode_NUL, amode_NUL, amode_NUL, amode_ABX, amode_ABX, amode_NUL,
	amode_IMM, amode_XID, amode_NUL, amode_NUL, amode_ZPG, amode_ZPG, amode_ZPG, amode_NUL,
	amode_NUL, amode_IMM, amode_NUL, amode_NUL, amode_ABS, amode_ABS, amode_ABS, amode_NUL,
	amode_REL, amode_IDY, amode_NUL, amode_NUL, amode_NUL, amode_ZPX, amode_ZPX, amode_NUL,
	amode_NUL, amode_ABY, amode_NUL, amode_NUL, amode_NUL, amode_ABX, amode_ABX, amode_NUL
};

#endif
#ifndef CPU_H
#define CPU_H

#include <stdint.h>

/* Ricoh 2A03 (MOH 6502) */

/*
Opcodes:
Source
    http://www.thealmightyguru.com/Games/Hacking/Wiki/index.php?title=6502_Opcodes

Hex 	Opcode 	Mode
03 		
04 		
07 		
0B 		
0C 		
0F 		
13 		
14 		
17 		
1A 		
1B 		
1C 		
1F 		
23 		
27 		 	
2B 		
2F 		
33 		
34 		
37 			
3A 		
3B 		
3C 		
3F 		
43 		
44 		
47 		
4B 		
4F 		
53 		
54 		
57 		
5A 		
5B 		
5C 		
5F 		
63 		
64 		
67 		
6B 		
6F 		
73 		
74 		
77 		
7A 		
7B 		
7C 		
7F 		
80 		
82 		
83 		
87 		
89 		
8B 		
8F 		
93 		
97 		
9B 		
9C 		
9E 		
9F 		
A3 		
A7 		
AB 		
AF 		
B3 		
B7 		
BB 		
BF 		
C2 		
C3 		
C7 		
CB 		
CF 		
D3 		
D4 		
D7 		
DA 		
DB 		
DC 		
DF 		
E2 		
E3 		
E7 		
EB 		
EF 		
F3 		
F4 		
F7 		
FA 		
FB 		
FC 		
FF 		*/



typedef struct
{
	uint16_t pc;		/* program counter */ /* int may be faster */
	uint8_t sp;			/* stack pointer */
	uint8_t p;			/* processor flags */
	uint8_t a, x, y;	/* registers */
} CPU;

uint8_t stack[255];
uint8_t ram[2048]; /* TODO: MEMORY MAPPING */

void cpu_reset(CPU* cpu);

/*** TODO: cycle counting ***/
/*** TODO: reuse functions ***/
/*** TODO: make static/inline? ***/

/* Load/store operations*/
void cpu_LDA(CPU* cpu, uint16_t address);
void cpu_LDX(CPU* cpu, uint16_t address);
void cpu_LDY(CPU* cpu, uint16_t address);
void cpu_STA(CPU* cpu, uint16_t address);
void cpu_STX(CPU* cpu, uint16_t address);
void cpu_STY(CPU* cpu, uint16_t address);

/* Register transfers */
void cpu_TAX(CPU* cpu, uint16_t address);
void cpu_TAY(CPU* cpu, uint16_t address);
void cpu_TXA(CPU* cpu, uint16_t address);
void cpu_TYA(CPU* cpu, uint16_t address);

/* Stack operations */
void cpu_TSX(CPU* cpu, uint16_t address);
void cpu_TXS(CPU* cpu, uint16_t address);
void cpu_PHA(CPU* cpu, uint16_t address);
void cpu_PHA(CPU* cpu, uint16_t address);
void cpu_PHP(CPU* cpu, uint16_t address);
void cpu_PLA(CPU* cpu, uint16_t address);
void cpu_PLP(CPU* cpu, uint16_t address);

/* Logical operations */
void cpu_AND(CPU* cpu, uint16_t address);
void cpu_EOR(CPU* cpu, uint16_t address);
void cpu_ORA(CPU* cpu, uint16_t address);
void cpu_BIT(CPU* cpu, uint16_t address);

/* Arithmetic operations */
void cpu_ADC(CPU* cpu, uint16_t address);
void cpu_SBC(CPU* cpu, uint16_t address);
void cpu_CMP(CPU* cpu, uint16_t address);
void cpu_CPX(CPU* cpu, uint16_t address);
void cpu_CPY(CPU* cpu, uint16_t address);

/* Increments and decrements */
void cpu_INC(CPU* cpu, uint16_t address);
void cpu_INX(CPU* cpu, uint16_t address);
void cpu_INY(CPU* cpu, uint16_t address);
void cpu_DEC(CPU* cpu, uint16_t address);
void cpu_DEX(CPU* cpu, uint16_t address);
void cpu_DEY(CPU* cpu, uint16_t address);

/* Shifts */
void cpu_ASL(CPU* cpu, uint16_t address);
void cpu_LSR(CPU* cpu, uint16_t address);
void cpu_ROL(CPU* cpu, uint16_t address);
void cpu_ROR(CPU* cpu, uint16_t address);

/* Jumps and calls */
void cpu_JMP(CPU* cpu, uint16_t address);
void cpu_JSR(CPU* cpu, uint16_t address);
void cpu_RTS(CPU* cpu, uint16_t address);

/* Conditional branches */
void cpu_BCC(CPU* cpu, uint16_t address);
void cpu_BCS(CPU* cpu, uint16_t address);
void cpu_BEQ(CPU* cpu, uint16_t address);
void cpu_BMI(CPU* cpu, uint16_t address);
void cpu_BNE(CPU* cpu, uint16_t address);
void cpu_BPL(CPU* cpu, uint16_t address);
void cpu_BVC(CPU* cpu, uint16_t address);
void cpu_BVS(CPU* cpu, uint16_t address);

/* Status flag changes */
void cpu_CLC(CPU* cpu, uint16_t address);
void cpu_CLD(CPU* cpu, uint16_t address);
void cpu_CLI(CPU* cpu, uint16_t address);
void cpu_CLV(CPU* cpu, uint16_t address);
void cpu_SEC(CPU* cpu, uint16_t address);
void cpu_SED(CPU* cpu, uint16_t address);
void cpu_SEI(CPU* cpu, uint16_t address);

/* System */
void cpu_BRK(CPU* cpu, uint16_t address); /*TODO: brk (sort of) HAS AN OPERAND (second byte is padding)*/
void cpu_NOP(CPU* cpu, uint16_t address);
void cpu_RTI(CPU* cpu, uint16_t address);
void cpu_KIL(CPU* cpu, uint16_t address);

static void (*opcodes[256])(CPU*, uint16_t) = 
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

typedef enum
{
	mNUL,	/* null (undefined opcodes. may implement later) */
	mIMP,	/* implied */
	mACC,	/* accumulator */
	mIMM,	/* immediate */
	mZPG,	/* zero page */
	mZPX,	/* zero page, x-indexed */
	mZPY,	/* zero page, y-indexed */
	mREL,	/* relative */
	mABS,	/* absolute */
	mABX,	/* absolute, x-indexed */
	mABY,	/* absolute, y-indexed */
	mIND,	/* indirect */
	mXID,	/* x-indexed, indirect */
	mIDY,	/* indirect, y-indexed */
} mode;

static mode modes[256] =
{
	mIMP, mXID, mNUL, mNUL, mNUL, mZPG, mZPG, mNUL,
	mIMP, mIMM, mACC, mNUL, mNUL, mABS, mABS, mNUL,
	mREL, mIDY, mNUL, mNUL, mNUL, mZPX, mZPX, mNUL,
	mIMP, mABY, mNUL, mNUL, mNUL, mABX, mABX, mNUL,
	mABS, mXID, mNUL, mNUL, mZPG, mZPG, mZPG, mNUL,
	mIMP, mIMM, mACC, mNUL, mABS, mABS, mABS, mNUL,
	mREL, mIDY, mNUL, mNUL, mNUL, mZPX, mZPX, mNUL,
	mIMP, mABY, mNUL, mNUL, mNUL, mABX, mABX, mNUL,
	mIMP, mXID, mNUL, mNUL, mNUL, mZPG, mZPG, mNUL,
	mIMP, mIMM, mACC, mNUL, mABS, mABS, mABS, mNUL,
	mREL, mIDY, mNUL, mNUL, mNUL, mZPX, mZPX, mNUL,
	mIMP, mABY, mNUL, mNUL, mNUL, mABX, mABX, mNUL,
	mIMP, mXID, mNUL, mNUL, mNUL, mZPG, mZPG, mNUL,
	mIMP, mIMM, mACC, mNUL, mIND, mABS, mABS, mNUL,
	mREL, mIDY, mNUL, mNUL, mNUL, mZPX, mZPX, mNUL,
	mIMP, mABY, mNUL, mNUL, mNUL, mABX, mABX, mNUL,
	mNUL, mXID, mNUL, mNUL, mZPG, mZPG, mZPG, mNUL,
	mIMP, mNUL, mIMP, mNUL, mABS, mABS, mABS, mNUL,
	mREL, mIDY, mNUL, mNUL, mZPX, mZPX, mZPY, mNUL,
	mIMP, mABY, mIMP, mNUL, mNUL, mABX, mNUL, mNUL,
	mIMM, mXID, mIMM, mNUL, mZPG, mZPG, mZPG, mNUL,
	mIMP, mIMM, mIMP, mNUL, mABS, mABS, mABS, mNUL,
	mREL, mIDY, mNUL, mNUL, mZPX, mZPX, mZPY, mNUL,
	mIMP, mABY, mIMP, mNUL, mABX, mABX, mABY, mNUL,
	mIMM, mXID, mNUL, mNUL, mZPG, mZPG, mZPG, mNUL,
	mIMP, mIMM, mIMP, mNUL, mABS, mABS, mABS, mNUL,
	mREL, mIDY, mNUL, mNUL, mNUL, mZPX, mZPX, mNUL,
	mIMP, mABY, mNUL, mNUL, mNUL, mABX, mABX, mNUL,
	mIMM, mXID, mNUL, mNUL, mZPG, mZPG, mZPG, mNUL,
	mIMP, mIMM, mIMP, mNUL, mABS, mABS, mABS, mNUL,
	mREL, mIDY, mNUL, mNUL, mNUL, mZPX, mZPX, mNUL,
	mIMP, mABY, mNUL, mNUL, mNUL, mABX, mABX, mNUL
};

#endif
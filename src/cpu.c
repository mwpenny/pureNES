#include "cpu.h"


/* TODO: USE THESE

#define SET_CRY(x)	x | 0x01
#define SET_ZRO(x)	x | 0x02
#define SET_INT(x)	x | 0x04
#define SET_DEC(x)	x | 0x08
#define SET_BRK(x)	x | 0x10
#define SET_NUL(x)	x | 0x20
#define SET_OVR(x)	x | 0x40
#define SET_NEG(x)	x | 0x80

#define CLR_CRY(x)	x & 0xFE
#define CLR_ZER(x)	x & 0xFD
#define CLR_INT(x)	x & 0xFB
#define CLR_DEC(x)	x & 0xF7
#define CLR_BRK(x)	x & 0xEF
#define CLR_NUL(x)	x & 0xDF
#define CLR_OVR(x)	x & 0xBF
#define CLR_NEG(x)	x & 0x7F

*/

/* TODO: FIX + INDIRECT JUMP BUG?*/
uint16_t cpu_normalize_addr(CPU* cpu, uint8_t* opcode)
{
	mode m = modes[*opcode];
	uint16_t address;

	switch (modes[*opcode])
	{
	case mNUL:
	case mIMP:
	case mACC:
		address = 0;
		break;
	case mIMM:
		address = cpu->pc+1;
		break;
	case mZPG:
		address = opcode[1] % 0xFF;
		break;
	case mZPX:
		address = (opcode[1] + cpu->x) % 0xFF;
		break;
	case mZPY:
		address = (opcode[1] + cpu->y) % 0xFF;
		break;
	case mREL:
		address = cpu->pc + opcode[1];
		break;
	case mABS:
		address = opcode[1] & (opcode[2] << 8);
		break;
	case mABX:
		address = (opcode[1] & (opcode[2] << 8)) + cpu->x;
		break;
	case mABY:
		address = (opcode[1] & (opcode[2] << 8)) + cpu->y;
		break;
	case mIND:
		address = ram[(opcode[1] & (opcode[2] << 8))];
		break;
	case mXID:
		address = ram[((opcode[1] & (opcode[2] << 8)) + cpu->x) % 0xFF];
		break;
	case mIDY:
		address = (ram[(opcode[1] & (opcode[2] << 8))] + cpu->y) % 0xFF;
		break;
	}

	return address;
}

void exec(CPU* cpu, uint8_t* opcode)
{
	opcodes[*opcode](cpu, cpu_normalize_addr(cpu, opcode));
}

void cpu_reset(CPU* cpu)
{
	cpu->p = 0x34; /* unused, break, and IRQ disable flags (0b00110100) */
	cpu->a = cpu->x = cpu->y = 0;
	cpu->sp = 0x00;

	/* TODO: don't actually push anything? */
	/* push pc */
	stack[cpu->sp--] = (cpu->pc-1) >> 8;
	stack[cpu->sp--] = (cpu->pc-1) & 0xFF;

	/* push p */
	cpu_PHP(cpu);

	/* sp should be 0xFD */

	/* jump to reset vector */
	cpu_JMP(cpu, ram[0xFFFC] & (ram[0xFFFD] << 8));


	/* TODO: init memory */
	/* after reset, IRQ disable flag set to true (ORed with 0x04) */
	/* after reset, APU is silenced */
}

void cpu_chk_aflags(CPU* cpu, uint8_t val)
{
	/* negative */
	if (val & 0x80) cpu->p |= 0x80;
	else cpu->p &= 0x7F;

	/* zero */
	if (!val) cpu->p |= 0x02;
	else cpu->p &= 0xFD;
}

/*** Load/store operations ***/

/* LDA - load accumulator */
void cpu_LDA(CPU* cpu, uint16_t address)
{
	cpu->a = ram[address];
	cpu_chk_aflags(cpu, cpu->a);
}

/* LDX - load x register */
void cpu_LDX(CPU* cpu, uint16_t address)
{
	cpu->x = ram[address];
	cpu_chk_aflags(cpu, cpu->a);
}

/* LDY - load y register */
void cpu_LDY(CPU* cpu, uint16_t address)
{
	cpu->y = ram[address];
	cpu_chk_aflags(cpu, cpu->a);
}

/* STA - store accumulator */
void cpu_STA(CPU* cpu, uint16_t address)
{
	ram[address] = cpu->a;
}

/* STX - store x register */
void cpu_STX(CPU* cpu, uint16_t address)
{
	ram[address] = cpu->x;
}

/* STY - store y register */
void cpu_STY(CPU* cpu, uint16_t address)
{
	ram[address] = cpu->y;
}


/*** Register transfers ***/

/* TAX - transfer accumulator to x */
void cpu_TAX(CPU* cpu)
{
	cpu->x = cpu->a;
	cpu_chk_aflags(cpu, cpu->a);
}

/* TAY - transfer accumulator to y */
void cpu_TAY(CPU* cpu)
{
	cpu->y = cpu->a;
	cpu_chk_aflags(cpu, cpu->a);
}

/* TXA - transfer x to accumulator */
void cpu_TXA(CPU* cpu)
{
	cpu->a = cpu->x;
	cpu_chk_aflags(cpu, cpu->a);
}

/* TYA - transfer y to accumulator */
void cpu_TYA(CPU* cpu)
{
	cpu->a = cpu->y;
	cpu_chk_aflags(cpu, cpu->a);
}


/*** Stack operations ***/

/* TSX - transfer stack pointer to x */
void cpu_TSX(CPU* cpu)
{
	cpu->x = cpu->sp;
	cpu_chk_aflags(cpu, cpu->a);
}

/* TXS - transfer x to stack pointer */
void cpu_TXS(CPU* cpu)
{
	cpu->sp = cpu->x;
}

/* PHA - push accumulator on stack */
void cpu_PHA(CPU* cpu)
{
	stack[cpu->sp--] = cpu->a;
}

/* PHP - push processor status on stack */
void cpu_PHP(CPU* cpu)
{
	stack[cpu->sp--] = cpu->p;
}

/* PLA - pull accumulator from stack */
void cpu_PLA(CPU* cpu)
{
	cpu->a = stack[++cpu->sp];
	cpu_chk_aflags(cpu, cpu->a);
}

/* PLP - pull processor status from stack */
void cpu_PLP(CPU* cpu)
{
	cpu->p = stack[++cpu->sp];
}


/*** Logical operations ***/

/* AND - logical AND with accumulator */
void cpu_AND(CPU* cpu, uint16_t address)
{
	cpu->a &= ram[address];
	cpu_chk_aflags(cpu, cpu->a);
}

/* EOR - exclusive OR with accumulator */
void cpu_EOR(CPU* cpu, uint16_t address)
{
	cpu->a ^= ram[address];
	cpu_chk_aflags(cpu, cpu->a);
}

/* ORA - inclusive OR with accumulator */
void cpu_ORA(CPU* cpu, uint16_t address)
{
	cpu->a |= ram[address];
	cpu_chk_aflags(cpu, cpu->a);
}

/* BIT - bit test with mask pattern in accumulator */
void cpu_BIT(CPU* cpu, uint16_t address) /* TODO: without branching? */
{
	if ((cpu->a & ram[address])) cpu->p &= 0xFD; /* zero flag */
	else cpu->p |= 0x02;
	cpu->p &= 0x3F; /* clear bits 6 and 7 */
	cpu->p |= (ram[address] & 0xC0); /* copy bits 6 and 7 from the memory location */
}


/*** Arithmetic operations ***/

/* ADC - add with carry to accumulator */
void cpu_ADC(CPU* cpu, uint16_t address)
{
	long val = cpu->a + ram[address] + (cpu->p & 0x01); /* long --> min 32 bits */
	if (val & 0x100) cpu_SEC(cpu);
	else cpu_CLC(cpu);

	if ((~(cpu->a ^ ram[address]) & (cpu->a ^ val)) & 0x80)
		cpu->p |= 0x40;
	else
		cpu_CLV(cpu);

	cpu->a = val;
	cpu_chk_aflags(cpu, cpu->a);
}

/* SBC - subtract with carry from accumulator */
void cpu_SBC(CPU* cpu, uint16_t address)
{
	cpu_ADC(cpu, ~ram[address]); /* -ram[address] - 1 */
}

/* CMP - compare with accumulator */
void cpu_CMP(CPU* cpu, uint16_t address)
{
	uint8_t val = cpu->a - ram[address];
	if (val >= 0) cpu_SEC(cpu);
	else cpu_CLC(cpu);
	cpu_chk_aflags(cpu, cpu->a);
}

/* CPX - compare with x register */
void cpu_CPX(CPU* cpu, uint16_t address)
{
	uint8_t val = cpu->x - ram[address];
	if (val >= 0) cpu_SEC(cpu);
	else cpu_CLC(cpu);
	cpu_chk_aflags(cpu, cpu->a);
}

/* CPY - compare with y register */
void cpu_CPY(CPU* cpu, uint16_t address)
{
	uint8_t val = cpu->y - ram[address];
	if (val >= 0) cpu_SEC(cpu);
	else cpu_CLC(cpu);
	cpu_chk_aflags(cpu, cpu->a);
}


/*** Increments and decrements ***/

/* INC - increment memory location */
void cpu_INC(CPU* cpu, uint16_t address)
{
	++ram[address];
	cpu_chk_aflags(cpu, cpu->a);
}

/* INX - increment x register */
void cpu_INX(CPU* cpu)
{
	++cpu->x;
	cpu_chk_aflags(cpu, cpu->a);
}

/* INY - increment y register */
void cpu_INY(CPU* cpu)
{
	++cpu->y;
	cpu_chk_aflags(cpu, cpu->a);
}

/* DEC - decrement memory location */
void cpu_DEC(CPU* cpu, uint16_t address)
{
	--ram[address];
	cpu_chk_aflags(cpu, cpu->a);
}

/* DEX - decrement x register */
void cpu_DEX(CPU* cpu)
{
	--cpu->x;
	cpu_chk_aflags(cpu, cpu->a);
}

/* DEY - recrement y register */
void cpu_DEY(CPU* cpu)
{
	--cpu->y;
	cpu_chk_aflags(cpu, cpu->a);
}


/*** Shifts ***/

/* ASL - arithmetic shift left */
void cpu_ASL(CPU* cpu, uint16_t address)
{
	uint8_t* val; /* TODO: determine if A or M */
	cpu->p |= (*val & 0x80) >> 7;
	*val <<= 1;
	cpu_chk_aflags(cpu, cpu->a);
}

/* LSR - logical shift right */
void cpu_LSR(CPU* cpu, uint16_t address)
{
	uint8_t* val; /* TODO: determine if A or M */
	cpu->p |= *val & 0x01;
	*val >>= 1;
	cpu_chk_aflags(cpu, cpu->a);
}

/* ROL - rotate left */
void cpu_ROL(CPU* cpu, uint16_t address)
{
	uint8_t* val; /* TODO: determine if A or M */
	uint8_t carry = cpu->p & 0x01;
	cpu->p |= (*val & 0x80) >> 7;
	*val <<= 1;
	*val |= carry;
	cpu_chk_aflags(cpu, cpu->a);
}

/* ROR - rotate right */
void cpu_ROR(CPU* cpu, uint16_t address)
{
	uint8_t* val; /* TODO: determine if A or M */
	uint8_t carry = cpu->p & 0x01;
	cpu->p |= *val & 0x01;
	*val >>= 1;
	*val |= carry << 7;
	cpu_chk_aflags(cpu, cpu->a);
}


/*** Jumps and calls ***/

/* JMP - jump to location */
void cpu_JMP(CPU* cpu, uint16_t address)
{
	cpu->pc = address;
}

/* JSR - jump to subroutine */
void cpu_JSR(CPU* cpu, uint16_t address)
{
	stack[cpu->sp--] = (cpu->pc-1) >> 8;
	stack[cpu->sp--] = (cpu->pc-1) & 0xFF;
	cpu->pc = address;
}

/* RTS - return from subroutine */
void cpu_RTS(CPU* cpu)
{
	cpu->pc = (stack[++cpu->sp] | (stack[++cpu->sp] << 8)) + 1;
}


/*** Conditional branches ***/

/* BCC - branch if carry flag clear */
void cpu_BCC(CPU* cpu, uint16_t address)
{
	if (!(cpu->p & 0x01)) cpu->pc = address;
}

/* BCS - branch if carry flag set */
void cpu_BCS(CPU* cpu, uint16_t address)
{
	if (cpu->p & 0x01) cpu->pc = address;
}

/* BEQ - branch if zero flag set */
void cpu_BEQ(CPU* cpu, uint16_t address)
{
	if (cpu->p & 0x02) cpu->pc = address;
}

/* BMI - branch if negative flag set */
void cpu_BMI(CPU* cpu, uint16_t address)
{
	if (cpu->p & 0x80) cpu->pc = address;
}

/* BNE - branch if zero flag clear */
void cpu_BNE(CPU* cpu, uint16_t address)
{
	if (!(cpu->p & 0x02)) cpu->pc = address;
}

/* BPL - branch if negative flag set */
void cpu_BPL(CPU* cpu, uint16_t address)
{
	if (!(cpu->p & 0x80)) cpu->pc = address;
}

/* BVC - branch if overflow flag clear */
void cpu_BVC(CPU* cpu, uint16_t address)
{
	if (!(cpu->p & 0x40)) cpu->pc = address;
}

/* BVC - branch if overflow flag set */
void cpu_BVS(CPU* cpu, uint16_t address)
{
	if (cpu->p & 0x40) cpu->pc = address;
}


/*** Status flag changes ***/

/* CLC - clear carry flag */
void cpu_CLC(CPU* cpu)
{
	cpu->p &= 0xFE;
}

/* CLD - clear decimal mode flag */
void cpu_CLD(CPU* cpu)
{
	cpu->p &= 0xF7;
}

/* CLI - clear interrupt disable flag */
void cpu_CLI(CPU* cpu)
{
	cpu->p &= 0xFB;
}

/* CLV - clear overflow flag */
void cpu_CLV(CPU* cpu)
{
	cpu->p &= 0xBF;
}

/* SEC - set carry flag */
void cpu_SEC(CPU* cpu)
{
	cpu->p |= 0x01;
}

/* SED - set decimal mode flag */
void cpu_SED(CPU* cpu)
{
	cpu->p |= 0x08;
}

/* SEI - set interrupt disable flag */
void cpu_SEI(CPU* cpu)
{
	cpu->p |= 0x04;
}


/*** System instructions ***/

/* BRK - interrupt */
void cpu_BRK(CPU* cpu)
{
	stack[cpu->sp--] = (cpu->pc) >> 8;
	stack[cpu->sp--] = (cpu->pc) & 0xFF;
	cpu_PHP(cpu);
	cpu_SEI(cpu);
	cpu->pc = ram[0xFFFE] & (ram[0xFFFF] << 8);
}

/* NOP - no operation */
void cpu_NOP(CPU* cpu) {}

/* RTI - return from interrupt */
void cpu_RTI(CPU* cpu)
{
	cpu_PLP(cpu);
	cpu->pc = stack[++cpu->sp] | (stack[++cpu->sp] << 8);
}


/*** Undocumented instructions ***/

/* KIL - crash the processor */
void cpu_KIL(CPU* cpu)
{
	/* TODO: CRASH PROCESSOR */
}
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

void exec(unsigned char opcode)
{
	opcodes[opcode];
}

/* TODO: FIX + INDIRECT JUMP BUG?*/
unsigned short cpu_normalize_addr(CPU* cpu, unsigned char* opcode)
{
	mode m = modes[*opcode];
	unsigned short address;

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

void exec(CPU* cpu, unsigned char* opcode)
{
	opcodes[*opcode](cpu, cpu_normalize_addr(cpu, opcode));
}

void cpu_reset(CPU* cpu)
{
	cpu->p = 0x34; /* IRQ disabled */ /* 0b00100000 */
	cpu->a = cpu->x = cpu->y = 0;
	cpu->sp = 0xFD;
	/* TODO: set PC to address at 0xFFFC and 0xFFFD (reset vector) */

	/* TODO: init memory */
	/* after reset, SP is decremented by 3 (that rhymed) */
	/* after reset, IRQ disable flag set to true (ORed with 0x04) */
	/* after reset, APU is silenced */
}

void cpu_chk_neg(CPU* cpu, unsigned char val)
{
	if (val & 0x80) cpu->p |= 0x80;
	else cpu->p &= 0x7F;
}

void cpu_chk_zer(CPU* cpu, unsigned char val)
{
	if (!val) cpu->p |= 0x02;
	else cpu->p &= 0xFD;
}


/* Load/store operations*/
void cpu_LDA(CPU* cpu, unsigned short address)
{
	cpu->a = ram[address];
	cpu_chk_zer(cpu, cpu->a);
	cpu_chk_neg(cpu, cpu->a);
}
void cpu_LDX(CPU* cpu, unsigned short address)
{
	cpu->x = ram[address];
	cpu_chk_zer(cpu, cpu->x);
	cpu_chk_neg(cpu, cpu->x);
}
void cpu_LDY(CPU* cpu, unsigned short address)
{
	cpu->y = ram[address];
	cpu_chk_zer(cpu, cpu->y);
	cpu_chk_neg(cpu, cpu->y);
}
void cpu_STA(CPU* cpu, unsigned short address)
{
	ram[address] = cpu->a;
}
void cpu_STX(CPU* cpu, unsigned short address)
{
	ram[address] = cpu->x;
}
void cpu_STY(CPU* cpu, unsigned short address)
{
	ram[address] = cpu->y;
}

/* Register transfers */
void cpu_TAX(CPU* cpu)
{
	cpu->x = cpu->a;
	cpu_chk_zer(cpu, cpu->x);
	cpu_chk_neg(cpu, cpu->x);
}
void cpu_TAY(CPU* cpu)
{
	cpu->y = cpu->a;
	cpu_chk_zer(cpu, cpu->y);
	cpu_chk_neg(cpu, cpu->y);
}
void cpu_TXA(CPU* cpu) /* TODO: POTENTIAL OPTIMIZATION: function to transfer arbitrary values to a */
{
	cpu->a = cpu->x;
	cpu_chk_zer(cpu, cpu->a);
	cpu_chk_neg(cpu, cpu->a);
}
void cpu_TYA(CPU* cpu)
{
	cpu->a = cpu->y;
	cpu_chk_zer(cpu, cpu->a);
	cpu_chk_neg(cpu, cpu->a);
}

/* Stack operations */
void cpu_TSX(CPU* cpu)
{
	cpu->x = cpu->sp;
	cpu_chk_zer(cpu, cpu->x);
	cpu_chk_neg(cpu, cpu->x);
}
void cpu_TXS(CPU* cpu)
{
	cpu->sp = cpu->x;
}
void cpu_PHA(CPU* cpu)
{
	stack[cpu->sp--] = cpu->a;
}
void cpu_PHP(CPU* cpu)
{
	stack[cpu->sp--] = cpu->p;
}
void cpu_PLA(CPU* cpu)
{
	cpu->a = stack[++cpu->sp];
	cpu_chk_zer(cpu, cpu->a);
	cpu_chk_neg(cpu, cpu->a);
}
void cpu_PLP(CPU* cpu)
{
	cpu->p = stack[++cpu->sp];
}

/* Logical operations */
void cpu_AND(CPU* cpu, unsigned short address)
{
	cpu->a &= ram[address];
	cpu_chk_zer(cpu, cpu->a);
	cpu_chk_neg(cpu, cpu->a);
}
void cpu_EOR(CPU* cpu, unsigned short address)
{
	cpu->a ^= ram[address];
	cpu_chk_zer(cpu, cpu->a);
	cpu_chk_neg(cpu, cpu->a);
}
void cpu_ORA(CPU* cpu, unsigned short address)
{
	cpu->a |= ram[address];
	cpu_chk_zer(cpu, cpu->a);
	cpu_chk_neg(cpu, cpu->a);
}
void cpu_BIT(CPU* cpu, unsigned short address) /* TODO: without branching? */
{
	if ((cpu->a & ram[address])) cpu->p &= 0xFD;
	else cpu->p |= 0x02;
	cpu->p &= 0x3F; /* clear bits 6 and 7 */
	cpu->p |= (ram[address] & 0xC0); /* copy bits 6 and 7 from the memory location */
}

/* Arithmetic operations */
void cpu_ADC(CPU* cpu, unsigned short address)
{
	int val = cpu->a + ram[address] + (cpu->p & 0x01);
	if (val > 0xFF) cpu_SEC(cpu);
	else cpu_CLC(cpu);

	if (!((cpu->a ^ ram[address]) & 0x80) && ((cpu->a ^ val) & 0x80))
		cpu->p |= 0x40;
	else
		cpu_CLV(cpu);

	cpu->a = val;
	cpu_chk_zer(cpu, cpu->a);
	cpu_chk_neg(cpu, cpu->a);
}
void cpu_SBC(CPU* cpu, unsigned short address)
{
	int val = cpu->a - ram[address] - ~(cpu->p & 0x01);
	if (val > 0xFF) cpu_CLC(cpu);
	else cpu_SEC(cpu);

	if (((cpu->a ^ ram[address]) & 0x80) && ((cpu->a ^ val) & 0x80))
		cpu->p |= 0x40;
	else
		cpu_CLV(cpu);

	cpu->a = val;
	cpu_chk_zer(cpu, cpu->a);
	cpu_chk_neg(cpu, cpu->a);
}

/* TODO: generic compare function */
void cpu_CMP(CPU* cpu, unsigned short address)
{
	unsigned char val = cpu->a - ram[address];
	if (val >= 0) cpu_SEC(cpu);
	else cpu_CLC(cpu);
	cpu_chk_zer(cpu, val);
	cpu_chk_neg(cpu, val);
}
void cpu_CPX(CPU* cpu, unsigned short address)
{
	unsigned char val = cpu->x - ram[address];
	if (val >= 0) cpu_SEC(cpu);
	else cpu_CLC(cpu);
	cpu_chk_zer(cpu, val);
	cpu_chk_neg(cpu, val);
}
void cpu_CPY(CPU* cpu, unsigned short address)
{
	unsigned char val = cpu->y - ram[address];
	if (val >= 0) cpu_SEC(cpu);
	else cpu_CLC(cpu);
	cpu_chk_zer(cpu, val);
	cpu_chk_neg(cpu, val);
}

/* Increments and decrements */
/* TODO: generic inc/dec function? */
void cpu_INC(CPU* cpu, unsigned short address)
{
	++ram[address];
	cpu_chk_zer(cpu, ram[address]);
	cpu_chk_neg(cpu, ram[address]);
}
void cpu_INX(CPU* cpu)
{
	++cpu->x;
	cpu_chk_zer(cpu, cpu->x);
	cpu_chk_neg(cpu, cpu->x);
}
void cpu_INY(CPU* cpu)
{
	++cpu->y;
	cpu_chk_zer(cpu, cpu->y);
	cpu_chk_neg(cpu, cpu->y);
}
void cpu_DEC(CPU* cpu, unsigned short address)
{
	--ram[address];
	cpu_chk_zer(cpu, ram[address]);
	cpu_chk_neg(cpu, ram[address]);
}
void cpu_DEX(CPU* cpu)
{
	--cpu->x;
	cpu_chk_zer(cpu, cpu->x);
	cpu_chk_neg(cpu, cpu->x);
}
void cpu_DEY(CPU* cpu)
{
	--cpu->y;
	cpu_chk_zer(cpu, cpu->y);
	cpu_chk_neg(cpu, cpu->y);
}

/* Shifts */
void cpu_ASL(CPU* cpu, unsigned short address)
{
	unsigned char* val; /* TODO: determine if A or M */
	cpu->p |= (*val & 0x80) >> 7;
	*val <<= 1;
	cpu_chk_zer(cpu, *val);
	cpu_chk_neg(cpu, *val);
}
void cpu_LSR(CPU* cpu, unsigned short address)
{
	unsigned char* val; /* TODO: determine if A or M */
	cpu->p |= *val & 0x01;
	*val >>= 1;
	cpu->p &= 0x7F;
	cpu_chk_zer(cpu, *val);
	cpu_chk_neg(cpu, *val);
}
void cpu_ROL(CPU* cpu, unsigned short address)
{
	unsigned char* val; /* TODO: determine if A or M */
	unsigned char carry = cpu->p & 0x01;
	cpu->p |= (*val & 0x80) >> 7;
	*val <<= 1;
	*val |= carry;
	cpu_chk_zer(cpu, *val);
	cpu_chk_neg(cpu, *val);
}
void cpu_ROR(CPU* cpu, unsigned short address)
{
	unsigned char* val; /* TODO: determine if A or M */
	unsigned char carry = cpu->p & 0x01;
	cpu->p |= *val & 0x01;
	*val >>= 1;
	*val |= carry << 7;
	cpu_chk_zer(cpu, *val);
	cpu_chk_neg(cpu, *val);
}

/* Jumps and calls */
void cpu_JMP(CPU* cpu, unsigned short address)
{
	cpu->pc = address;
}
void cpu_JSR(CPU* cpu, unsigned short address)
{
	stack[cpu->sp--] = (cpu->pc-1) >> 8;
	stack[cpu->sp--] = (cpu->pc-1) & 0xFF;
	cpu->pc = address;
}
void cpu_RTS(CPU* cpu)
{
	cpu->pc = stack[++cpu->sp] | (stack[++cpu->sp] << 8) + 1;
}

/* Conditional branches */
void cpu_BCC(CPU* cpu, unsigned short address)
{
	if (!(cpu->p & 0x01)) cpu->pc = address;
}
void cpu_BCS(CPU* cpu, unsigned short address)
{
	if (cpu->p & 0x01) cpu->pc = address;
}
void cpu_BEQ(CPU* cpu, unsigned short address)
{
	if (cpu->p & 0x02) cpu->pc = address;
}
void cpu_BMI(CPU* cpu, unsigned short address)
{
	if (cpu->p & 0x80) cpu->pc = address;
}
void cpu_BNE(CPU* cpu, unsigned short address)
{
	if (!(cpu->p & 0x02)) cpu->pc = address;
}
void cpu_BPL(CPU* cpu, unsigned short address)
{
	if (!(cpu->p & 0x80)) cpu->pc = address;
}
void cpu_BVC(CPU* cpu, unsigned short address)
{
	if (!(cpu->p & 0x40)) cpu->pc = address;
}
void cpu_BVS(CPU* cpu, unsigned short address)
{
	if (cpu->p & 0x40) cpu->pc = address;
}

/* Status flag changes */
void cpu_CLC(CPU* cpu)
{
	cpu->p &= 0xFE;
}
void cpu_CLD(CPU* cpu)
{
	cpu->p &= 0xF7;
}
void cpu_CLI(CPU* cpu)
{
	cpu->p &= 0xFB;
}
void cpu_CLV(CPU* cpu)
{
	cpu->p &= 0xBF;
}
void cpu_SEC(CPU* cpu)
{
	cpu->p |= 0x01;
}
void cpu_SED(CPU* cpu)
{
	cpu->p |= 0x08;
}
void cpu_SEI(CPU* cpu)
{
	cpu->p |= 0x04;
}

/* System */
void cpu_BRK(CPU* cpu)
{
	stack[cpu->sp--] = (cpu->pc) >> 8;
	stack[cpu->sp--] = (cpu->pc) & 0xFF;
	cpu_PHP(cpu);
	cpu_SEI(cpu);
	cpu->pc = 0xFFFE; /* TODO: endianness */
}
void cpu_NOP(CPU* cpu) {}
void cpu_RTI(CPU* cpu)
{
	cpu_PLP(cpu);
	cpu->pc = stack[++cpu->sp] | (stack[++cpu->sp] << 8);
}
void cpu_KIL(CPU* cpu)
{
	/* TODO: CRASH PROCESSOR */
}
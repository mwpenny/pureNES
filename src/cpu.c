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

/* TODO: INDIRECT JUMP BUG?*/
void cpu_get_oc_operand(CPU* cpu, OCInfo* oci)
{
	/* interpret opcode operand based on addressing mode */
	switch (modes[oci->opcode])
	{
	case mNUL:
	case mIMP:
	case mACC:
		oci->operand = 0;
		break;
	case mIMM:
		oci->operand = cpu->pc+1;
		break;
	case mZPG:
		oci->operand = memory_get(cpu->pc+1) % 0xFF;
		break;
	case mZPX:
		oci->operand = (memory_get(cpu->pc+1) + cpu->x) % 0xFF;
		break;
	case mZPY:
		oci->operand = (memory_get(cpu->pc+1) + cpu->y) % 0xFF;
		break;
	case mREL:
	{
		uint16_t ofs = memory_get(cpu->pc+1);
		oci->operand = cpu->pc + 2;
		oci->operand += ofs & 0x80 ? (ofs - 0x100) : ofs; /* account for negative offset */
		break;
	}
	case mABS:
		oci->operand = memory_get16(cpu->pc+1);
		break;
	case mABX:
		oci->operand = memory_get16(cpu->pc+1) + cpu->x;
		break;
	case mABY:
		oci->operand = memory_get16(cpu->pc+1) + cpu->y;
		break;
	case mIND:
		oci->operand = memory_get16(memory_get16(cpu->pc+1));
		break;
	case mXID:
		oci->operand = memory_get16((memory_get(cpu->pc+1) + cpu->x) % 0xFF);
		break;
	case mIDY:
		oci->operand = (memory_get16(cpu->pc+1) + cpu->y) % 0xFF;
		break;
	}
}
#include <stdio.h>
void cpu_tick(CPU* cpu, FILE* log)
{
	OCInfo oci;
	oci.opcode = memory_get(cpu->pc);
	cpu_get_oc_operand(cpu, &oci);
	/* fprintf(log, "PC:%x\tOPCODE:%s\tOPERAND:%x\tBYTES:%x\n", cpu->pc, oc_names[oci.opcode], oci.operand, oc_sizes[oci.opcode]); */
	printf("PC:%x\tOPCODE:%s\tOPERAND:%x\tBYTES:%x\n", cpu->pc, oc_names[oci.opcode], oci.operand, oc_sizes[oci.opcode]);
	cpu->pc += oc_sizes[oci.opcode];
	opcodes[oci.opcode](cpu, &oci);
}

void cpu_reset(CPU* cpu)
{
	cpu->p = 0x34; /* unused, break, and IRQ disable flags (0b00110100) */
	cpu->a = cpu->x = cpu->y = 0;
	cpu->sp = 0x00;

	/* TODO: don't actually push anything? */
	/* push pc */
	memory_set(0x100 | cpu->sp--, (cpu->pc) >> 8);
	memory_set(0x100 | cpu->sp--, (cpu->pc) & 0xFF);

	/* push p */
	memory_set(0x100 | cpu->sp--, cpu->p);

	/* sp should now be 0xFD */

	/* jump to reset vector */
	cpu->pc = memory_get16(0xFFFC);


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
void cpu_LDA(CPU* cpu, OCInfo* oci)
{
	cpu->a = memory_get(oci->operand);
	cpu_chk_aflags(cpu, cpu->a);
}

/* LDX - load x register */
void cpu_LDX(CPU* cpu, OCInfo* oci)
{
	cpu->x = memory_get(oci->operand);
	cpu_chk_aflags(cpu, cpu->a);
}

/* LDY - load y register */
void cpu_LDY(CPU* cpu, OCInfo* oci)
{
	cpu->y = memory_get(oci->operand);
	cpu_chk_aflags(cpu, cpu->a);
}

/* STA - store accumulator */
void cpu_STA(CPU* cpu, OCInfo* oci)
{
	memory_set(oci->operand, cpu->a);
}

/* STX - store x register */
void cpu_STX(CPU* cpu, OCInfo* oci)
{
	memory_set(oci->operand, cpu->x);
}

/* STY - store y register */
void cpu_STY(CPU* cpu, OCInfo* oci)
{
	memory_set(oci->operand, cpu->y);
}


/*** Register transfers ***/

/* TAX - transfer accumulator to x */
void cpu_TAX(CPU* cpu, OCInfo* oci)
{
	cpu->x = cpu->a;
	cpu_chk_aflags(cpu, cpu->a);
}

/* TAY - transfer accumulator to y */
void cpu_TAY(CPU* cpu, OCInfo* oci)
{
	cpu->y = cpu->a;
	cpu_chk_aflags(cpu, cpu->a);
}

/* TXA - transfer x to accumulator */
void cpu_TXA(CPU* cpu, OCInfo* oci)
{
	cpu->a = cpu->x;
	cpu_chk_aflags(cpu, cpu->a);
}

/* TYA - transfer y to accumulator */
void cpu_TYA(CPU* cpu, OCInfo* oci)
{
	cpu->a = cpu->y;
	cpu_chk_aflags(cpu, cpu->a);
}


/*** Stack operations ***/

/* TSX - transfer stack pointer to x */
void cpu_TSX(CPU* cpu, OCInfo* oci)
{
	cpu->x = cpu->sp;
	cpu_chk_aflags(cpu, cpu->a);
}

/* TXS - transfer x to stack pointer */
void cpu_TXS(CPU* cpu, OCInfo* oci)
{
	cpu->sp = cpu->x;
}

/* PHA - push accumulator on stack */
void cpu_PHA(CPU* cpu, OCInfo* oci)
{
	memory_set(0x100 | cpu->sp--, cpu->a);	
}

/* PHP - push processor status on stack */
void cpu_PHP(CPU* cpu, OCInfo* oci)
{
	memory_set(0x100 | cpu->sp--, cpu->p);
}

/* PLA - pull accumulator from stack */
void cpu_PLA(CPU* cpu, OCInfo* oci)
{
	cpu->a = memory_get(0x100 | ++cpu->sp);
	cpu_chk_aflags(cpu, cpu->a);
}

/* PLP - pull processor status from stack */
void cpu_PLP(CPU* cpu, OCInfo* oci)
{
	cpu->p =  memory_get(0x100 | ++cpu->sp);
}


/*** Logical operations ***/

/* AND - logical AND with accumulator */
void cpu_AND(CPU* cpu, OCInfo* oci)
{
	cpu->a &= memory_get(oci->operand);
	cpu_chk_aflags(cpu, cpu->a);
}

/* EOR - exclusive OR with accumulator */
void cpu_EOR(CPU* cpu, OCInfo* oci)
{
	cpu->a ^= memory_get(oci->operand);
	cpu_chk_aflags(cpu, cpu->a);
}

/* ORA - inclusive OR with accumulator */
void cpu_ORA(CPU* cpu, OCInfo* oci)
{
	cpu->a |= memory_get(oci->operand);
	cpu_chk_aflags(cpu, cpu->a);
}

/* BIT - bit test with mask pattern in accumulator */
void cpu_BIT(CPU* cpu, OCInfo* oci) /* TODO: without branching? */
{
	if ((cpu->a & memory_get(oci->operand))) cpu->p &= 0xFD; /* zero flag */
	else cpu->p |= 0x02;
	cpu->p &= 0x3F; /* clear bits 6 and 7 */
	cpu->p |= (memory_get(oci->operand) & 0xC0); /* copy bits 6 and 7 from the memory location */
}


/*** Arithmetic operations ***/

/* ADC - add with carry to accumulator */
void cpu_ADC(CPU* cpu, OCInfo* oci)
{
	long val = cpu->a + memory_get(oci->operand) + (cpu->p & 0x01); /* long --> min 32 bits */
	if (val & 0x100) cpu_SEC(cpu, oci);
	else cpu_CLC(cpu, oci);

	if ((~(cpu->a ^ memory_get(oci->operand)) & (cpu->a ^ val)) & 0x80)
		cpu->p |= 0x40;
	else
		cpu_CLV(cpu, oci);

	cpu->a = (uint8_t)val;
	cpu_chk_aflags(cpu, cpu->a);
}

/* SBC - subtract with carry from accumulator */
void cpu_SBC(CPU* cpu, OCInfo* oci)
{
	oci->operand = ~memory_get(oci->operand);
	cpu_ADC(cpu, oci); /* -operand - 1 */
}

/* CMP - compare with accumulator */
void cpu_CMP(CPU* cpu, OCInfo* oci)
{
	uint8_t val = cpu->a - memory_get(oci->operand);
	if (val >= 0) cpu_SEC(cpu, oci);
	else cpu_CLC(cpu, oci);
	cpu_chk_aflags(cpu, cpu->a);
}

/* CPX - compare with x register */
void cpu_CPX(CPU* cpu, OCInfo* oci)
{
	uint8_t val = cpu->x - memory_get(oci->operand);
	if (val >= 0) cpu_SEC(cpu, oci);
	else cpu_CLC(cpu, oci);
	cpu_chk_aflags(cpu, cpu->a);
}

/* CPY - compare with y register */
void cpu_CPY(CPU* cpu, OCInfo* oci)
{
	uint8_t val = cpu->y - memory_get(oci->operand);
	if (val >= 0) cpu_SEC(cpu, oci);
	else cpu_CLC(cpu, oci);
	cpu_chk_aflags(cpu, cpu->a);
}


/*** Increments and decrements ***/

/* INC - increment memory location */
void cpu_INC(CPU* cpu, OCInfo* oci)
{
	memory_set(oci->operand, memory_get(oci->operand)+1);
	cpu_chk_aflags(cpu, cpu->a);
}

/* INX - increment x register */
void cpu_INX(CPU* cpu, OCInfo* oci)
{
	++cpu->x;
	cpu_chk_aflags(cpu, cpu->a);
}

/* INY - increment y register */
void cpu_INY(CPU* cpu, OCInfo* oci)
{
	++cpu->y;
	cpu_chk_aflags(cpu, cpu->a);
}

/* DEC - decrement memory location */
void cpu_DEC(CPU* cpu, OCInfo* oci)
{
	memory_set(oci->operand, memory_get(oci->operand)-1);
	cpu_chk_aflags(cpu, cpu->a);
}

/* DEX - decrement x register */
void cpu_DEX(CPU* cpu, OCInfo* oci)
{
	--cpu->x;
	cpu_chk_aflags(cpu, cpu->a);
}

/* DEY - recrement y register */
void cpu_DEY(CPU* cpu, OCInfo* oci)
{
	--cpu->y;
	cpu_chk_aflags(cpu, cpu->a);
}


/*** Shifts ***/

/* ASL - arithmetic shift left */
void cpu_ASL(CPU* cpu, OCInfo* oci)
{
	/* opcode == 0x0A --> use accumulator value */
	uint8_t val = oci->opcode == 0x0A ? cpu->a : memory_get(oci->operand);
	cpu->p |= (val & 0x80) >> 7;
	val <<= 1;
	cpu_chk_aflags(cpu, cpu->a);
}

/* LSR - logical shift right */
void cpu_LSR(CPU* cpu, OCInfo* oci)
{
	/* opcode == 0x0A --> use accumulator value */
	uint8_t val = oci->opcode == 0x0A ? cpu->a : memory_get(oci->operand);
	cpu->p |= val & 0x01;
	val >>= 1;
	cpu_chk_aflags(cpu, cpu->a);
}

/* ROL - rotate left */
void cpu_ROL(CPU* cpu, OCInfo* oci)
{
	/* opcode == 0x0A --> use accumulator value */
	uint8_t val = oci->opcode == 0x0A ? cpu->a : memory_get(oci->operand);
	uint8_t carry = cpu->p & 0x01;
	cpu->p |= (val & 0x80) >> 7;
	val <<= 1;
	val |= carry;
	cpu_chk_aflags(cpu, cpu->a);
}

/* ROR - rotate right */
void cpu_ROR(CPU* cpu, OCInfo* oci)
{
	/* opcode == 0x0A --> use accumulator value */
	uint8_t val = oci->opcode == 0x0A ? cpu->a : memory_get(oci->operand);
	uint8_t carry = cpu->p & 0x01;
	cpu->p |= val & 0x01;
	val >>= 1;
	val |= carry << 7;
	cpu_chk_aflags(cpu, cpu->a);
}


/*** Jumps and calls ***/

/* JMP - jump to location */
void cpu_JMP(CPU* cpu, OCInfo* oci)
{
	cpu->pc = oci->operand;
}

/* JSR - jump to subroutine */
void cpu_JSR(CPU* cpu, OCInfo* oci)
{
	memory_set(0x100 | cpu->sp--, (cpu->pc-1) >> 8);
	memory_set(0x100 | cpu->sp--, (cpu->pc-1) & 0xFF);
	cpu->pc = oci->operand;
}

/* RTS - return from subroutine */
void cpu_RTS(CPU* cpu, OCInfo* oci)
{
	cpu->pc = (memory_get(0x100 | ++cpu->sp) | (memory_get(0x100 | ++cpu->sp) << 8)) + 1;
}


/*** Conditional branches ***/

/* BCC - branch if carry flag clear */
void cpu_BCC(CPU* cpu, OCInfo* oci)
{
	if (!(cpu->p & 0x01)) cpu->pc = oci->operand;
}

/* BCS - branch if carry flag set */
void cpu_BCS(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & 0x01) cpu->pc = oci->operand;
}

/* BEQ - branch if zero flag set */
void cpu_BEQ(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & 0x02) cpu->pc = oci->operand;
}

/* BMI - branch if negative flag set */
void cpu_BMI(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & 0x80) cpu->pc = oci->operand;
}

/* BNE - branch if zero flag clear */
void cpu_BNE(CPU* cpu, OCInfo* oci)
{
	if (!(cpu->p & 0x02)) cpu->pc = oci->operand;
}

/* BPL - branch if negative flag set */
void cpu_BPL(CPU* cpu, OCInfo* oci)
{
	if (!(cpu->p & 0x80)) cpu->pc = oci->operand;
}

/* BVC - branch if overflow flag clear */
void cpu_BVC(CPU* cpu, OCInfo* oci)
{
	if (!(cpu->p & 0x40)) cpu->pc = oci->operand;
}

/* BVC - branch if overflow flag set */
void cpu_BVS(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & 0x40) cpu->pc = oci->operand;
}


/*** Status flag changes ***/

/* CLC - clear carry flag */
void cpu_CLC(CPU* cpu, OCInfo* oci)
{
	cpu->p &= 0xFE;
}

/* CLD - clear decimal mode flag */
void cpu_CLD(CPU* cpu, OCInfo* oci)
{
	cpu->p &= 0xF7;
}

/* CLI - clear interrupt disable flag */
void cpu_CLI(CPU* cpu, OCInfo* oci)
{
	cpu->p &= 0xFB;
}

/* CLV - clear overflow flag */
void cpu_CLV(CPU* cpu, OCInfo* oci)
{
	cpu->p &= 0xBF;
}

/* SEC - set carry flag */
void cpu_SEC(CPU* cpu, OCInfo* oci)
{
	cpu->p |= 0x01;
}

/* SED - set decimal mode flag */
void cpu_SED(CPU* cpu, OCInfo* oci)
{
	cpu->p |= 0x08;
}

/* SEI - set interrupt disable flag */
void cpu_SEI(CPU* cpu, OCInfo* oci)
{
	cpu->p |= 0x04;
}


/*** System instructions ***/

/* BRK - interrupt */
void cpu_BRK(CPU* cpu, OCInfo* oci)
{
	memory_set(0x100 | cpu->sp--, (cpu->pc) >> 8);
	memory_set(0x100 | cpu->sp--, (cpu->pc) & 0xFF);
	cpu_PHP(cpu, oci);
	cpu_SEI(cpu, oci);
	cpu->pc = memory_get16(0xFFFE);
}

/* NOP - no operation */
void cpu_NOP(CPU* cpu, OCInfo* oci) {}

/* RTI - return from interrupt */
void cpu_RTI(CPU* cpu, OCInfo* oci)
{
	cpu_PLP(cpu, oci);
	cpu->pc = (memory_get(0x100 | ++cpu->sp) | (memory_get(0x100 | ++cpu->sp) << 8));
}


/*** Undocumented instructions ***/

/* KIL - crash the processor */
void cpu_KIL(CPU* cpu, OCInfo* oci)
{
	/* TODO: CRASH PROCESSOR */
}
#include "cpu.h"

/*** Addressing mode handlers ***/
void amode_NUL(CPU* cpu, OCInfo* oci)
{
	/* handles null (undefined), implied, and accumulator addressing modes */
	oci->operand = 0;
}
void amode_IMM(CPU* cpu, OCInfo* oci)
{
	oci->operand = cpu->pc+1;
	++cpu->pc;
}
void amode_ZPG(CPU* cpu, OCInfo* oci)
{
	oci->operand = memory_get(cpu->pc+1) % 0xFF;
	++cpu->pc;
}
void amode_ZPX(CPU* cpu, OCInfo* oci)
{
	oci->operand = (memory_get(cpu->pc+1) + cpu->x) % 0xFF;
	++cpu->pc;
}
void amode_ZPY(CPU* cpu, OCInfo* oci)
{
	oci->operand = (memory_get(cpu->pc+1) + cpu->y) % 0xFF;
	++cpu->pc;
}
void amode_REL(CPU* cpu, OCInfo* oci)
{
	uint16_t ofs = memory_get(cpu->pc+1);
	oci->operand = cpu->pc + 2;
	oci->operand += ofs & MASK_N ? (ofs - 0x100) : ofs; /* account for negative offset */
	++cpu->pc;
}
void amode_ABS(CPU* cpu, OCInfo* oci)
{
	oci->operand = memory_get16(cpu->pc+1);
	cpu->pc += 2;
}
void amode_ABX(CPU* cpu, OCInfo* oci)
{
	oci->operand = memory_get16(cpu->pc+1) + cpu->x;
	cpu->pc += 2;
}
void amode_ABY(CPU* cpu, OCInfo* oci)
{
	oci->operand = memory_get16(cpu->pc+1) + cpu->y;
	cpu->pc += 2;
}
/* TODO: INDIRECT JUMP BUG?*/
void amode_IND(CPU* cpu, OCInfo* oci)
{
	oci->operand = memory_get16_ind(memory_get16(cpu->pc+1));
	cpu->pc += 2;
}
void amode_XID(CPU* cpu, OCInfo* oci)
{
	oci->operand = memory_get16_ind((memory_get(cpu->pc+1) + cpu->x) % 0xFF);
	++cpu->pc;
}
void amode_IDY(CPU* cpu, OCInfo* oci)
{
	oci->operand = (memory_get16_ind(cpu->pc+1) + cpu->y) % 0xFF;
	cpu->pc += 2;
}

#include <stdio.h>
void cpu_tick(CPU* cpu, FILE* log)
{
	OCInfo oci;
	oci.opcode = memory_get(cpu->pc);
	printf("%x\t%s ", cpu->pc, oc_names[oci.opcode]);
	amodes[oci.opcode](cpu, &oci);
	/* fprintf(log, "PC:%x\tOPCODE:%s\tOPERAND:%x\tBYTES:%x\n", cpu->pc, oc_names[oci.opcode], oci.operand, oc_sizes[oci.opcode]); */
	printf("$%x\n", oci.operand);
	++cpu->pc;
	opcodes[oci.opcode](cpu, &oci);
}

void cpu_reset(CPU* cpu)
{
	cpu->p = 0x20 | MASK_B | MASK_I; /* unused, break, and IRQ disable flags */
	cpu->a = cpu->x = cpu->y = 0;
	cpu->sp = 0;

	/* TODO: don't actually push anything? */
	/* push pc */
	memory_set(0x100 | cpu->sp--, (cpu->pc) >> 8);
	memory_set(0x100 | cpu->sp--, (cpu->pc) & 0xFF);

	/* push p */
	cpu_PHP(cpu, NULL);

	/* sp should now be 0xFD */

	/* jump to reset vector */
	cpu->pc = memory_get16(ADDR_RESET);


	/* TODO: init memory */
	/* after reset, IRQ disable flag set to true (ORed with 0x04) */
	/* after reset, APU is silenced */
}

void cpu_chk_aflags(CPU* cpu, uint8_t val)
{
	/* set/clear zero and negative flags */
	if (val & MASK_Z) cpu->p |= MASK_Z;
	else cpu->p &= ~MASK_Z;

	if (val & MASK_N) cpu->p |= MASK_N;
	else cpu->p &= ~MASK_N;
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
	/* clear zero flag if register a has a mask bit set */
	if ((cpu->a & memory_get(oci->operand))) cpu->p &= ~MASK_Z;
	else cpu->p |= MASK_Z;

	cpu->p &= ~(MASK_N | MASK_N); /* clear bits 6 and 7 */
	cpu->p |= (memory_get(oci->operand) & (MASK_N | MASK_N)); /* copy bits 6 and 7 from the memory location */
}


/*** Arithmetic operations ***/
/* TODO: BCD MODE */

/* ADC - add with carry to accumulator */
void cpu_ADC(CPU* cpu, OCInfo* oci)
{
	long val = cpu->a + memory_get(oci->operand) + (cpu->p & MASK_C); /* long --> min 32 bits */
	if (val & 0x100) cpu_SEC(cpu, oci);
	else cpu_CLC(cpu, oci);

	if ((~(cpu->a ^ memory_get(oci->operand)) & (cpu->a ^ val)) & MASK_N)
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
	/* address == 0 --> use accumulator */
	uint8_t* loc = ~oci->operand ? &cpu->a : memory_get_mapped(oci->operand);
	cpu_CLC(cpu, NULL);
	cpu->p |= (*loc & 0x80) >> 7;
	*loc <<= 1;
	cpu_chk_aflags(cpu, *loc);
}

/* LSR - logical shift right */
void cpu_LSR(CPU* cpu, OCInfo* oci)
{
	/* address == 0 --> use accumulator */
	uint8_t* loc = ~oci->operand ? &cpu->a : memory_get_mapped(oci->operand);
	cpu_CLC(cpu, NULL);
	cpu->p |= *loc & MASK_C;
	*loc >>= 1;
	cpu_chk_aflags(cpu, *loc);
}

/* ROL - rotate left */
void cpu_ROL(CPU* cpu, OCInfo* oci)
{
	/* address == 0 --> use accumulator */
	uint8_t* loc = ~oci->operand ? &cpu->a : memory_get_mapped(oci->operand);
	uint8_t carry = cpu->p & MASK_C;
	cpu_CLC(cpu, NULL);
	cpu->p |= (*loc & 0x80) >> 7;
	*loc <<= 1;
	*loc |= carry;
	cpu_chk_aflags(cpu, *loc);
}

/* ROR - rotate right */
void cpu_ROR(CPU* cpu, OCInfo* oci)
{
	/* address == 0 --> use accumulator */
	uint8_t* loc = ~oci->operand ? &cpu->a : memory_get_mapped(oci->operand);
	uint8_t carry = cpu->p & MASK_C;
	cpu_CLC(cpu, NULL);
	cpu->p |= *loc & MASK_C;
	*loc >>= 1;
	*loc |= carry << 7;
	cpu_chk_aflags(cpu, *loc);
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
	cpu->pc = memory_get16(0x100 | ++cpu->sp) + 1;
	++cpu->sp;
}


/*** Conditional branches ***/

/* BCC - branch if carry flag clear */
void cpu_BCC(CPU* cpu, OCInfo* oci)
{
	if (~(cpu->p & MASK_C)) cpu->pc = oci->operand;
}

/* BCS - branch if carry flag set */
void cpu_BCS(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & MASK_C) cpu->pc = oci->operand;
}

/* BEQ - branch if zero flag set */
void cpu_BEQ(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & MASK_Z) cpu->pc = oci->operand;
}

/* BMI - branch if negative flag set */
void cpu_BMI(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & MASK_N) cpu->pc = oci->operand;
}

/* BNE - branch if zero flag clear */
void cpu_BNE(CPU* cpu, OCInfo* oci)
{
	if (~(cpu->p & MASK_Z)) cpu->pc = oci->operand;
}

/* BPL - branch if negative flag set */
void cpu_BPL(CPU* cpu, OCInfo* oci)
{
	if (~(cpu->p & MASK_N)) cpu->pc = oci->operand;
}

/* BVC - branch if overflow flag clear */
void cpu_BVC(CPU* cpu, OCInfo* oci)
{
	if (~(cpu->p & MASK_C)) cpu->pc = oci->operand;
}

/* BVC - branch if overflow flag set */
void cpu_BVS(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & MASK_V) cpu->pc = oci->operand;
}


/*** Status flag changes ***/

/* CLC - clear carry flag */
void cpu_CLC(CPU* cpu, OCInfo* oci)
{
	cpu->p &= ~MASK_C;
}

/* CLD - clear decimal mode flag */
void cpu_CLD(CPU* cpu, OCInfo* oci)
{
	cpu->p &= ~MASK_D;
}

/* CLI - clear interrupt disable flag */
void cpu_CLI(CPU* cpu, OCInfo* oci)
{
	cpu->p &= ~MASK_I;
}

/* CLV - clear overflow flag */
void cpu_CLV(CPU* cpu, OCInfo* oci)
{
	cpu->p &= ~MASK_V;
}

/* SEC - set carry flag */
void cpu_SEC(CPU* cpu, OCInfo* oci)
{
	cpu->p |= MASK_C;
}

/* SED - set decimal mode flag */
void cpu_SED(CPU* cpu, OCInfo* oci)
{
	cpu->p |= MASK_D;
}

/* SEI - set interrupt disable flag */
void cpu_SEI(CPU* cpu, OCInfo* oci)
{
	cpu->p |= MASK_I;
}


/*** System instructions ***/

/* BRK - interrupt */
void cpu_BRK(CPU* cpu, OCInfo* oci)
{
	memory_set(0x100 | cpu->sp--, (cpu->pc) >> 8);
	memory_set(0x100 | cpu->sp--, (cpu->pc) & 0xFF);
	cpu_PHP(cpu, oci);
	cpu_SEI(cpu, oci);
	cpu->pc = memory_get16(ADDR_BRK);
}

/* NOP - no operation */
void cpu_NOP(CPU* cpu, OCInfo* oci) {}

/* RTI - return from interrupt */
void cpu_RTI(CPU* cpu, OCInfo* oci)
{
	cpu_PLP(cpu, oci);
	cpu->pc = (memory_get(0x100 | ++cpu->sp) | (memory_get(0x100 | ++cpu->sp) << 8));
}


/*** TODO: Undocumented instructions ***/

/* KIL - crash the processor */
void cpu_KIL(CPU* cpu, OCInfo* oci)
{
	/* TODO: CRASH PROCESSOR */
}
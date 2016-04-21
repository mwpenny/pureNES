#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "nes.h"
#include "memory.h"

void cpu_init(CPU* cpu, NES* nes)
{
	memset(cpu, 0, sizeof(*cpu));
	cpu->cycles = 0;
	cpu->dma_cycles = 0;
	cpu->oddcycle = 0;
	cpu->nes = nes; /* set parent system */
}

void cpu_reset(CPU* cpu)
{
	/*cpu->p = 0x20 | MASK_B | MASK_I; /* unused, break, and IRQ disable flags *///
	cpu->p = 0x20 | MASK_I;
	cpu->a = cpu->x = cpu->y = 0;
	cpu->sp = 0;

	/* TODO: don't actually push anything? */
	/* push pc */
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc) >> 8);
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc) & 0xFF);

	/* push p */
	cpu_PHP(cpu, NULL);

	/* sp should now be 0xFD */

	/* jump to reset vector */
	cpu->pc = memory_get16(cpu->nes, ADDR_RESET);
	/*cpu->pc = 0xC000; /* for nestest */

	/* TODO: init memory */
	/* after reset, IRQ disable flag set to true (ORed with 0x04) */
	/* after reset, APU is silenced */
}

/* Jump to interrupt vector. Common behavior to NMI, IRQ, and BRK */
static void jmi(CPU* cpu, uint16_t vector) {
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc) >> 8);
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc) & 0xFF);
	cpu_PHP(cpu, NULL);
	cpu_SEI(cpu, NULL);
	cpu->pc = memory_get16(cpu->nes, vector);
}

static void handle_interrupt(CPU* cpu, uint8_t type)
{
	OCInfo oci = {0, 0};
	if (type == INT_NUL) return;

	/* Set jump address to interrupt handler */
	switch (type)
	{
	case INT_RST:
		cpu_reset(cpu);
		break;
	case INT_NMI:
		jmi(cpu, ADDR_NMI);
		cpu->cycles += 7;
		break;
	case INT_IRQ:
		/* Don't handle interrupt if IRQs are masked */
		if (!(cpu->p & MASK_I)) {
			jmi(cpu, ADDR_IRQ);
			cpu->cycles += 7;
		}
	}

	cpu->interrupt = INT_NUL;
}

#include <stdio.h>
int cpu_step(CPU* cpu)
{
	static int occount = 0;
	/*static FILE* log;*/
	OCInfo oci;

	cpu->cycles = 0;

	if (cpu->dma_cycles)
	{
		--cpu->dma_cycles;
		return 1;
	}

	if (cpu->interrupt != INT_NUL) {
		handle_interrupt(cpu, cpu->interrupt);
		return cpu->cycles;
	}

	/*log = fopen("cpu.log", "a");*/

	oci.opcode = memory_get(cpu->nes, cpu->pc++);
	/*printf("%X\t%s ", cpu->pc-1, oc_names[oci.opcode]);
	/*fprintf(log, "%X\t%s ", cpu->pc-1, oc_names[oci.opcode]);*/
	amodes[oci.opcode](cpu, &oci);
	/*fprintf(log, "PC:%x\tOPCODE:%s\tOPERAND:%x\tBYTES:%x\n", cpu->pc, oc_names[oci.opcode], oci.operand, instruction_cycles[oci.opcode]);*/
	/*fprintf(log, "$%.4x\t\t\t", oci.operand);*/
	/*printf("$%.4x\t\t\t", oci.operand);
	printf("A:%.2X X:%.2X Y:%.2X P:%.2X SP:%.2X\n", cpu->a, cpu->x, cpu->y, cpu->p, cpu->sp);

	/*fprintf(log, "A:%.2X X:%.2X Y:%.2X P:%.2X SP:%.2X\n", cpu->a, cpu->x, cpu->y, cpu->p, cpu->sp);
	fclose(log);*/

	cpu->cycles = instruction_cycles[oci.opcode];

	/* Used for adding another cycle during DMA writes */
	cpu->oddcycle = (cpu->oddcycle ^ cpu->cycles%2);

	opcodes[oci.opcode](cpu, &oci);
	return cpu->cycles;
}

void cpu_interrupt(CPU* cpu, uint8_t type)
{
	cpu->interrupt = type;
}

static void chk_aflags(CPU* cpu, int16_t val)
{
	/* set/clear zero and negative flags */
	if (!val) cpu->p |= MASK_Z;
	else cpu->p &= ~MASK_Z;

	if (val & MASK_N) cpu->p |= MASK_N;
	else cpu->p &= ~MASK_N;
}

/*** Addressing mode handlers ***/

/* Handles null (undefined), implicit, and accumulator addressing modes */
void amode_NUL(CPU* cpu, OCInfo* oci)
{
	oci->operand = 0;
}

/* Immediate addressing. Use next byte as the value */
void amode_IMM(CPU* cpu, OCInfo* oci)
{
	oci->operand = cpu->pc++;
}

/* Zero page addressing. Use the next byte as the address */
void amode_ZPG(CPU* cpu, OCInfo* oci)
{
	oci->operand = memory_get(cpu->nes, cpu->pc++) & 0xFF;
}

/* Zero page, X addressing. Use the next byte plus the value in the X register */
void amode_ZPX(CPU* cpu, OCInfo* oci)
{
	oci->operand = (memory_get(cpu->nes, cpu->pc++) + cpu->x) & 0xFF;
}

/* Zero page, Y addressing. Use the next byte plus the value in the Y register */
void amode_ZPY(CPU* cpu, OCInfo* oci)
{
	oci->operand = (memory_get(cpu->nes, cpu->pc++) + cpu->y) & 0xFF;
}

/* Relative addressing. Use PC plus the value of the next byte */
void amode_REL(CPU* cpu, OCInfo* oci)
{
	/* Get offset */
	oci->operand = memory_get(cpu->nes, cpu->pc++);
	/*oci->operand += (ofs & MASK_N) ? (ofs - 0x100) : ofs; /* account for negative offset */
}

/* Absolute addressing. Use the next two bytes */
void amode_ABS(CPU* cpu, OCInfo* oci)
{
	oci->operand = memory_get16(cpu->nes, cpu->pc++);
	++cpu->pc; /* for fetching pointer high byte */
}

/* Absolute, addressing helper function. Use the the next two bytes plus
   the value passed (from the appropriate register) */
static void amode_ABI(CPU* cpu, OCInfo* oci, uint8_t i)
{
	uint16_t addr = memory_get16(cpu->nes, cpu->pc++);
	++cpu->pc; /* for fecthing high byte of address */
	oci->operand = addr + i;

	/* Page boundary crosses cost one cycle */
	if ((addr & 0xFF00) != (oci->operand & 0xFF00))
		cpu->cycles += pagecross_cycles[oci->opcode];
}

/* Absolute, X addressing. Use the the next two bytes plus
   the value in the X register */
void amode_ABX(CPU* cpu, OCInfo* oci)
{
	amode_ABI(cpu, oci, cpu->x);
}

/* Absolute, Y addressing. Use the next two bytes plus the value
   in the Y register */
void amode_ABY(CPU* cpu, OCInfo* oci)
{
	amode_ABI(cpu, oci, cpu->y);
}

/* Indirect addressing. Use the address pointed to by the next 2 bytes */
void amode_IND(CPU* cpu, OCInfo* oci)
{
	oci->operand = memory_get16_ind(cpu->nes, memory_get16(cpu->nes, cpu->pc++));
	++cpu->pc; /* for fetching pointer high byte */
}

/* Indexed indirect addressing. Add the value in the X register to the next byte
   and use the address at that location (with zero page wraparound) */
void amode_XID(CPU* cpu, OCInfo* oci)
{
	oci->operand = memory_get16_ind(cpu->nes,
				  (memory_get(cpu->nes, cpu->pc++) + cpu->x) & 0xFF);
}

/* Indirect indexed addressing. Use the address pointed to by the next two bytes
   added to the value in the Y register (with zero page wraparound) */
void amode_IDY(CPU* cpu, OCInfo* oci)
{
	uint16_t addr = memory_get16_ind(cpu->nes, memory_get(cpu->nes, cpu->pc++) & 0xFF);
	oci->operand = (addr + cpu->y);

	/* add cycle for page cross */
	if ((addr & 0xFF00) != (oci->operand & 0xFF00))
		cpu->cycles += pagecross_cycles[oci->opcode];
}

/*** Load/store operations ***/

/* LDA - load accumulator */
void cpu_LDA(CPU* cpu, OCInfo* oci)
{
	cpu->a = memory_get(cpu->nes, oci->operand);
	chk_aflags(cpu, cpu->a);
}

/* LDX - load x register */
void cpu_LDX(CPU* cpu, OCInfo* oci)
{
	cpu->x = memory_get(cpu->nes, oci->operand);
	chk_aflags(cpu, cpu->x);
}

/* LDY - load y register */
void cpu_LDY(CPU* cpu, OCInfo* oci)
{
	cpu->y = memory_get(cpu->nes, oci->operand);
	chk_aflags(cpu, cpu->y);
}

/* STA - store accumulator */
void cpu_STA(CPU* cpu, OCInfo* oci)
{
	memory_set(cpu->nes, oci->operand, cpu->a);
}

/* STX - store x register */
void cpu_STX(CPU* cpu, OCInfo* oci)
{
	memory_set(cpu->nes, oci->operand, cpu->x);
}

/* STY - store y register */
void cpu_STY(CPU* cpu, OCInfo* oci)
{
	memory_set(cpu->nes, oci->operand, cpu->y);
}


/*** Register transfers ***/

/* TAX - transfer accumulator to x */
void cpu_TAX(CPU* cpu, OCInfo* oci)
{
	cpu->x = cpu->a;
	chk_aflags(cpu, cpu->x);
}

/* TAY - transfer accumulator to y */
void cpu_TAY(CPU* cpu, OCInfo* oci)
{
	cpu->y = cpu->a;
	chk_aflags(cpu, cpu->y);
}

/* TXA - transfer x to accumulator */
void cpu_TXA(CPU* cpu, OCInfo* oci)
{
	cpu->a = cpu->x;
	chk_aflags(cpu, cpu->a);
}

/* TYA - transfer y to accumulator */
void cpu_TYA(CPU* cpu, OCInfo* oci)
{
	cpu->a = cpu->y;
	chk_aflags(cpu, cpu->a);
}


/*** Stack operations ***/

/* TSX - transfer stack pointer to x */
void cpu_TSX(CPU* cpu, OCInfo* oci)
{
	cpu->x = cpu->sp;
	chk_aflags(cpu, cpu->x);
}

/* TXS - transfer x to stack pointer */
void cpu_TXS(CPU* cpu, OCInfo* oci)
{
	cpu->sp = cpu->x;
}

/* PHA - push accumulator on stack */
void cpu_PHA(CPU* cpu, OCInfo* oci)
{
	memory_set(cpu->nes, 0x100 | cpu->sp--, cpu->a);	
}

/* PHP - push processor status on stack */
void cpu_PHP(CPU* cpu, OCInfo* oci)
{
	/* BRK flag not set on hardware interrupts */
	uint8_t flags = cpu->p;
	if (cpu->interrupt != INT_IRQ && cpu->interrupt != INT_NMI)
		flags |= MASK_B;
	memory_set(cpu->nes, 0x100 | cpu->sp--, flags);
}

/* PLA - pull accumulator from stack */
void cpu_PLA(CPU* cpu, OCInfo* oci)
{
	cpu->a = memory_get(cpu->nes, 0x100 | ++cpu->sp);
	chk_aflags(cpu, cpu->a);
}

/* PLP - pull processor status from stack */
void cpu_PLP(CPU* cpu, OCInfo* oci)
{
	uint8_t p = memory_get(cpu->nes, 0x100 | ++cpu->sp);

	/* Discard BRK flag and set unused flag*/
	cpu->p = p & ~MASK_B | MASK_U;
}


/*** Logical operations ***/

/* AND - logical AND with accumulator */
void cpu_AND(CPU* cpu, OCInfo* oci)
{
	cpu->a &= memory_get(cpu->nes, oci->operand);
	chk_aflags(cpu, cpu->a);
}

/* EOR - exclusive OR with accumulator */
void cpu_EOR(CPU* cpu, OCInfo* oci)
{
	cpu->a ^= memory_get(cpu->nes, oci->operand);
	chk_aflags(cpu, cpu->a);
}

/* ORA - inclusive OR with accumulator */
void cpu_ORA(CPU* cpu, OCInfo* oci)
{
	cpu->a |= memory_get(cpu->nes, oci->operand);
	chk_aflags(cpu, cpu->a);
}

/* BIT - bit test with mask pattern in accumulator */
void cpu_BIT(CPU* cpu, OCInfo* oci) /* TODO: without branching? */
{
	uint8_t num = memory_get(cpu->nes, oci->operand);

	/* clear zero flag if register A has a mask bit set */
	if (cpu->a & num) cpu->p &= ~MASK_Z;
	else cpu->p |= MASK_Z;

	/* copy N and V bits from the memory location */
	cpu->p &= ~(MASK_N | MASK_V);
	cpu->p |= (num & (MASK_N | MASK_V));
}


/*** Arithmetic operations ***/
/* TODO: BCD MODE? */

/* ADC helper function. Works with a number directly */
static void adc(CPU* cpu, uint8_t num, OCInfo* oci)
{
	int16_t val = cpu->a + num + (cpu->p & MASK_C);
	if (val & 0x100) cpu_SEC(cpu, oci);
	else cpu_CLC(cpu, oci);

	/* set overflow flag if sum of 2 numbers with same sign has different sign */
	if ((~(cpu->a ^ num) & (cpu->a ^ val)) & MASK_N)
		cpu->p |= MASK_V;
	else
		cpu_CLV(cpu, oci);

	cpu->a = (uint8_t)val;
	chk_aflags(cpu, cpu->a);
}

/* ADC - add with carry to accumulator */
void cpu_ADC(CPU* cpu, OCInfo* oci)
{
	uint8_t num = memory_get(cpu->nes, oci->operand);
	adc(cpu, num, oci);
}

/* SBC - subtract with carry from accumulator */
void cpu_SBC(CPU* cpu, OCInfo* oci)
{
	/* -num - 1 */
	uint8_t num = ~memory_get(cpu->nes, oci->operand);
	adc(cpu, num, oci);
}

/* CMP - compare with accumulator */
void cpu_CMP(CPU* cpu, OCInfo* oci)
{
	int16_t val = cpu->a - memory_get(cpu->nes, oci->operand);
	if (val >= 0) cpu_SEC(cpu, oci);
	else cpu_CLC(cpu, oci);
	chk_aflags(cpu, val);
}

/* CPX - compare with x register */
void cpu_CPX(CPU* cpu, OCInfo* oci)
{
	int16_t val = cpu->x - memory_get(cpu->nes, oci->operand);
	if (val >= 0) cpu_SEC(cpu, oci);
	else cpu_CLC(cpu, oci);
	chk_aflags(cpu, val);
}

/* CPY - compare with y register */
void cpu_CPY(CPU* cpu, OCInfo* oci)
{
	int16_t val = cpu->y - memory_get(cpu->nes, oci->operand);
	if (val >= 0) cpu_SEC(cpu, oci);
	else cpu_CLC(cpu, oci);
	chk_aflags(cpu, val);
}


/*** Increments and decrements ***/

/* INC - increment memory location */
void cpu_INC(CPU* cpu, OCInfo* oci)
{
	uint8_t val = memory_get(cpu->nes, oci->operand)+1;
	memory_set(cpu->nes, oci->operand, val);
	chk_aflags(cpu, val);
}

/* INX - increment x register */
void cpu_INX(CPU* cpu, OCInfo* oci)
{
	++cpu->x;
	chk_aflags(cpu, cpu->x);
}

/* INY - increment y register */
void cpu_INY(CPU* cpu, OCInfo* oci)
{
	++cpu->y;
	chk_aflags(cpu, cpu->y);
}

/* DEC - decrement memory location */
void cpu_DEC(CPU* cpu, OCInfo* oci)
{
	uint8_t val = memory_get(cpu->nes, oci->operand)-1;
	memory_set(cpu->nes, oci->operand, val);
	chk_aflags(cpu, val);
}

/* DEX - decrement x register */
void cpu_DEX(CPU* cpu, OCInfo* oci)
{
	--cpu->x;
	chk_aflags(cpu, cpu->x);
}

/* DEY - decrement y register */
void cpu_DEY(CPU* cpu, OCInfo* oci)
{
	--cpu->y;
	chk_aflags(cpu, cpu->y);
}


/*** Shifts ***/

/* ASL - arithmetic shift left */
void cpu_ASL(CPU* cpu, OCInfo* oci)
{
	/* address == 0 --> use accumulator */
	uint8_t num = !oci->operand ? cpu->a : memory_get(cpu->nes, oci->operand);

	cpu_CLC(cpu, NULL);
	cpu->p |= (num & 0x80) >> 7;
	num <<= 1;
	chk_aflags(cpu, num);

	if (!oci->operand) cpu->a = num;
	else
		memory_set(cpu->nes, oci->operand, num);
}

/* LSR - logical shift right */
void cpu_LSR(CPU* cpu, OCInfo* oci)
{
	/* address == 0 --> use accumulator */
	uint8_t num = !oci->operand ? cpu->a : memory_get(cpu->nes, oci->operand);

	cpu_CLC(cpu, NULL);
	cpu->p |= num & MASK_C;
	num >>= 1;
	chk_aflags(cpu, num);

	if (!oci->operand) cpu->a = num;
	else
		memory_set(cpu->nes, oci->operand, num);
}

/* ROL - rotate left */
void cpu_ROL(CPU* cpu, OCInfo* oci)
{
	/* address == 0 --> use accumulator */
	uint8_t num = !oci->operand ? cpu->a : memory_get(cpu->nes, oci->operand);
	uint8_t carry = cpu->p & MASK_C;

	cpu_CLC(cpu, NULL);
	cpu->p |= (num & 0x80) >> 7;
	num <<= 1;
	num |= carry;
	chk_aflags(cpu, num);

	if (!oci->operand) cpu->a = num;
	else
		memory_set(cpu->nes, oci->operand, num);
}

/* ROR - rotate right */
void cpu_ROR(CPU* cpu, OCInfo* oci)
{
	/* address == 0 --> use accumulator */
	uint8_t num = !oci->operand ? cpu->a : memory_get(cpu->nes, oci->operand);
	uint8_t carry = cpu->p & MASK_C;

	cpu_CLC(cpu, NULL);
	cpu->p |= num & MASK_C;
	num >>= 1;
	num |= carry << 7;
	chk_aflags(cpu, num);

	if (!oci->operand) cpu->a = num;
	else
		memory_set(cpu->nes, oci->operand, num);
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
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc-1) >> 8);
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc-1) & 0xFF);
	cpu->pc = oci->operand;
}

/* RTS - return from subroutine */
void cpu_RTS(CPU* cpu, OCInfo* oci)
{
	cpu->pc = (memory_get(cpu->nes, 0x100 | ++cpu->sp) |
			  (memory_get(cpu->nes, 0x100 | ++cpu->sp) << 8)) + 1;
}


/*** Conditional branches ***/

static void branch(CPU* cpu, int8_t ofs)
{
	uint16_t addr = cpu->pc + ofs;

	/* Add cycle if branch is to another page. Not using lookup table
	   because the cycle penalty for every branch instruction is always
	   1. */
	if ((addr & 0xFF00) != (cpu->pc & 0xFF00))
		++cpu->cycles;

	/* Perform branch operation */
	cpu->pc = addr;
	++cpu->cycles;
}

/* BCC - branch if carry flag clear */
void cpu_BCC(CPU* cpu, OCInfo* oci)
{
	if (!(cpu->p & MASK_C))
		branch(cpu, (uint8_t)oci->operand);
}

/* BCS - branch if carry flag set */
void cpu_BCS(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & MASK_C)
		branch(cpu, (uint8_t)oci->operand);
}

/* BEQ - branch if zero flag set */
void cpu_BEQ(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & MASK_Z)
		branch(cpu, (uint8_t)oci->operand);
}

/* BMI - branch if negative flag set */
void cpu_BMI(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & MASK_N)
		branch(cpu, (uint8_t)oci->operand);
}

/* BNE - branch if zero flag clear */
void cpu_BNE(CPU* cpu, OCInfo* oci)
{
	if (!(cpu->p & MASK_Z))
		branch(cpu, (uint8_t)oci->operand);
}

/* BPL - branch if negative flag clear */
void cpu_BPL(CPU* cpu, OCInfo* oci)
{
	if (!(cpu->p & MASK_N))
		branch(cpu, (uint8_t)oci->operand);
}

/* BVC - branch if overflow flag clear */
void cpu_BVC(CPU* cpu, OCInfo* oci)
{
	if (!(cpu->p & MASK_V))
		branch(cpu, (uint8_t)oci->operand);
}

/* BVC - branch if overflow flag set */
void cpu_BVS(CPU* cpu, OCInfo* oci)
{
	if (cpu->p & MASK_V)
		branch(cpu, (uint8_t)oci->operand);
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
	jmi(cpu, ADDR_IRQ);
}

/* NOP - no operation */
void cpu_NOP(CPU* cpu, OCInfo* oci) {}

/* RTI - return from interrupt */
void cpu_RTI(CPU* cpu, OCInfo* oci)
{
	cpu_PLP(cpu, oci);
	cpu->p &= ~MASK_B; /* B flag only set on stack */
	cpu->pc = (memory_get(cpu->nes, 0x100 | ++cpu->sp) |
			  (memory_get(cpu->nes, 0x100 | ++cpu->sp) << 8));
}


/*** TODO: Undocumented instructions ***/

/* KIL - crash the processor */
void cpu_KIL(CPU* cpu, OCInfo* oci)
{
	/* TODO: CRASH PROCESSOR */
}
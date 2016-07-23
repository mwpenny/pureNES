#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "nes.h"
#include "memory.h"

/* Interrupt vectors */
#define ADDR_NMI 0xFFFA
#define ADDR_RESET 0xFFFC
#define ADDR_IRQ 0xFFFE

/* P flag bitmasks */
#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80


/*** CPU instructions ***/
static void update_flags(CPU* cpu, int16_t val)
{
	/* Set/clear zero and negative flags */
	if (val)
		cpu->p &= ~FLAG_Z;
	else
		cpu->p |= FLAG_Z;

	if (val & 0x80)
		cpu->p |= FLAG_N;
	else
		cpu->p &= ~FLAG_N;
}

/*** Status flag changes ***/
/* CLC - clear carry flag */
static void clc(CPU* cpu)
{
	cpu->p &= ~FLAG_C;
}

/* CLD - clear decimal mode flag */
static void cld(CPU* cpu)
{
	cpu->p &= ~FLAG_D;
}

/* CLI - clear interrupt disable flag */
static void cli(CPU* cpu)
{
	cpu->p &= ~FLAG_I;
}

/* CLV - clear overflow flag */
static void clv(CPU* cpu)
{
	cpu->p &= ~FLAG_V;
}

/* SEC - set carry flag */
static void sec(CPU* cpu)
{
	cpu->p |= FLAG_C;
}

/* SED - set decimal mode flag */
static void sed(CPU* cpu)
{
	cpu->p |= FLAG_D;
}

/* SEI - set interrupt disable flag */
static void sei(CPU* cpu)
{
	cpu->p |= FLAG_I;
}

/*** Load/store operations ***/
/* LDA - load accumulator */
static void lda(CPU* cpu)
{
	cpu->a = memory_get(cpu->nes, cpu->oc_addr);
	update_flags(cpu, cpu->a);
}

/* LDX - load x register */
static void ldx(CPU* cpu)
{
	cpu->x = memory_get(cpu->nes, cpu->oc_addr);
	update_flags(cpu, cpu->x);
}

/* LDY - load y register */
static void ldy(CPU* cpu)
{
	cpu->y = memory_get(cpu->nes, cpu->oc_addr);
	update_flags(cpu, cpu->y);
}

/* STA - store accumulator */
static void sta(CPU* cpu)
{
	memory_set(cpu->nes, cpu->oc_addr, cpu->a);
}

/* STX - store x register */
static void stx(CPU* cpu)
{
	memory_set(cpu->nes, cpu->oc_addr, cpu->x);
}

/* STY - store y register */
static void sty(CPU* cpu)
{
	memory_set(cpu->nes, cpu->oc_addr, cpu->y);
}

/*** Register transfers ***/
/* TAX - transfer accumulator to x */
static void tax(CPU* cpu)
{
	cpu->x = cpu->a;
	update_flags(cpu, cpu->x);
}

/* TAY - transfer accumulator to y */
static void tay(CPU* cpu)
{
	cpu->y = cpu->a;
	update_flags(cpu, cpu->y);
}

/* TXA - transfer x to accumulator */
static void txa(CPU* cpu)
{
	cpu->a = cpu->x;
	update_flags(cpu, cpu->a);
}

/* TYA - transfer y to accumulator */
static void tya(CPU* cpu)
{
	cpu->a = cpu->y;
	update_flags(cpu, cpu->a);
}

/*** Stack operations ***/
/* TSX - transfer stack pointer to x */
static void tsx(CPU* cpu)
{
	cpu->x = cpu->sp;
	update_flags(cpu, cpu->x);
}

/* TXS - transfer x to stack pointer */
static void txs(CPU* cpu)
{
	cpu->sp = cpu->x;
}

/* PHA - push accumulator on stack */
static void pha(CPU* cpu)
{
	memory_set(cpu->nes, 0x100 | cpu->sp--, cpu->a);	
}

/* PHP - push processor status on stack */
static void php(CPU* cpu)
{
	/* B flag set on PHP/BRK (software interrupts) */
	memory_set(cpu->nes, 0x100 | cpu->sp--, cpu->p | FLAG_B | FLAG_U);
}

/* PLA - pull accumulator from stack */
static void pla(CPU* cpu)
{
	cpu->a = memory_get(cpu->nes, 0x100 | ++cpu->sp);
	update_flags(cpu, cpu->a);
}

/* PLP - pull processor status from stack */
static void plp(CPU* cpu)
{
	/* B flag doesn't exist in hardware. Don't save to register */
	cpu->p = memory_get(cpu->nes, 0x100 | ++cpu->sp) & ~FLAG_B;
}

/*** Logical operations ***/
/* AND - logical AND with accumulator */
static void and(CPU* cpu)
{
	cpu->a &= memory_get(cpu->nes, cpu->oc_addr);
	update_flags(cpu, cpu->a);
}

/* EOR - exclusive OR with accumulator */
static void eor(CPU* cpu)
{
	cpu->a ^= memory_get(cpu->nes, cpu->oc_addr);
	update_flags(cpu, cpu->a);
}

/* ORA - inclusive OR with accumulator */
static void ora(CPU* cpu)
{
	cpu->a |= memory_get(cpu->nes, cpu->oc_addr);
	update_flags(cpu, cpu->a);
}

/* BIT - bit test with mask pattern in accumulator */
static void bit(CPU* cpu)
{
	uint8_t num = memory_get(cpu->nes, cpu->oc_addr);

	/* Clear zero flag if register A has a mask bit set */
	if (cpu->a & num)
		cpu->p &= ~FLAG_Z;
	else
		cpu->p |= FLAG_Z;

	/* Copy N and V bits from the memory location */
	cpu->p &= ~(FLAG_N | FLAG_V);
	cpu->p |= (num & (FLAG_N | FLAG_V));
}

/*** Arithmetic operations ***/
/* Addition helper function. Works with a number directly */
static void do_add(CPU* cpu, uint8_t num)
{
	int16_t val = cpu->a + num + (cpu->p & FLAG_C);
	if (val > 0xFF)
		sec(cpu);
	else
		clc(cpu);

	/* Sum of 2 numbers with same sign has different sign => overflow */
	if ((~(cpu->a ^ num) & (cpu->a ^ val)) & 0x80)
		cpu->p |= FLAG_V;
	else
		clv(cpu);

	cpu->a = (uint8_t)val;
	update_flags(cpu, cpu->a);
}

/* ADC - add with carry to accumulator */
static void adc(CPU* cpu)
{
	uint8_t num = memory_get(cpu->nes, cpu->oc_addr);
	do_add(cpu, num);
}

/* SBC - subtract with carry from accumulator */
static void sbc(CPU* cpu)
{
	/* -num - 1 */
	uint8_t num = ~memory_get(cpu->nes, cpu->oc_addr);
	do_add(cpu, num);
}

/* Comparison helper */
static void do_compare(CPU* cpu, uint8_t reg)
{
	uint8_t num = memory_get(cpu->nes, cpu->oc_addr);
	if (num > reg)
		clc(cpu);
	else
		sec(cpu);
	update_flags(cpu, reg - num);
}

/* CMP - compare with accumulator */
static void cmp(CPU* cpu)
{
	do_compare(cpu, cpu->a);
}

/* CPX - compare with x register */
static void cpx(CPU* cpu)
{
	do_compare(cpu, cpu->x);
}

/* CPY - compare with y register */
static void cpy(CPU* cpu)
{
	do_compare(cpu, cpu->y);
}

/*** Increments and decrements ***/
/* INC - increment value at memory location */
static void inc(CPU* cpu)
{
	uint8_t val = memory_get(cpu->nes, cpu->oc_addr)+1;
	memory_set(cpu->nes, cpu->oc_addr, val);
	update_flags(cpu, val);
}

/* INX - increment x register */
static void inx(CPU* cpu)
{
	++cpu->x;
	update_flags(cpu, cpu->x);
}

/* INY - increment y register */
static void iny(CPU* cpu)
{
	++cpu->y;
	update_flags(cpu, cpu->y);
}

/* DEC - decrement memory location */
static void dec(CPU* cpu)
{
	uint8_t val = memory_get(cpu->nes, cpu->oc_addr)-1;
	memory_set(cpu->nes, cpu->oc_addr, val);
	update_flags(cpu, val);
}

/* DEX - decrement x register */
static void dex(CPU* cpu)
{
	--cpu->x;
	update_flags(cpu, cpu->x);
}

/* DEY - decrement y register */
static void dey(CPU* cpu)
{
	--cpu->y;
	update_flags(cpu, cpu->y);
}

/*** Shifts ***/
/* Shift helper function. Gets number to shift based on addressing mode */
static uint8_t get_shift_num(CPU* cpu)
{
	if (cpu->instruction_amode == AMODE_ACC)
		return cpu->a;
	else
		return memory_get(cpu->nes, cpu->oc_addr);
}

/* Shift helper function. Stores shifted number to based on addressing mode */
static void store_shift_result(CPU* cpu, uint8_t num)
{
	if (cpu->instruction_amode == AMODE_ACC)
		cpu->a = num;
	else
		memory_set(cpu->nes, cpu->oc_addr, num);
}

/* ASL - arithmetic shift left */
static void asl(CPU* cpu)
{
	/* C <- num <- 0 */
	uint8_t num = get_shift_num(cpu);
	clc(cpu);
	cpu->p |= (num & 0x80) >> 7;
	num <<= 1;
	update_flags(cpu, num);
	store_shift_result(cpu, num);
}

/* LSR - logical shift right */
static void lsr(CPU* cpu)
{
	/* 0 -> num -> C */
	uint8_t num = get_shift_num(cpu);
	clc(cpu);
	cpu->p |= num & 0x01;
	num >>= 1;
	update_flags(cpu, num);
	store_shift_result(cpu, num);
}

/* ROL - rotate left */
static void rol(CPU* cpu)
{
	/* C <- num <- C */
	uint8_t num = get_shift_num(cpu);
	uint8_t carry = cpu->p & FLAG_C;
	clc(cpu);
	cpu->p |= (num & 0x80) >> 7;
	num <<= 1;
	num |= carry;
	update_flags(cpu, num);
	store_shift_result(cpu, num);
}

/* ROR - rotate right */
static void ror(CPU* cpu)
{
	/* C -> num -> C */
	uint8_t num = get_shift_num(cpu);
	uint8_t carry = cpu->p & FLAG_C;	
	clc(cpu);
	cpu->p |= num & FLAG_C;
	num >>= 1;
	num |= carry << 7;
	update_flags(cpu, num);
	store_shift_result(cpu, num);
}

/*** Jumps and calls ***/
/* JMP - jump to location */
static void jmp(CPU* cpu)
{
	cpu->pc = cpu->oc_addr;
}

/* JSR - jump to subroutine */
static void jsr(CPU* cpu)
{
	/* Push PC onto the stack */
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc-1) >> 8);
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc-1) & 0xFF);
	cpu->pc = cpu->oc_addr;
}

/* RTS - return from subroutine */
static void rts(CPU* cpu)
{
	/* Pull PC off the stack */
	cpu->pc = (memory_get(cpu->nes, 0x100 | ++cpu->sp) |
			  (memory_get(cpu->nes, 0x100 | ++cpu->sp) << 8)) + 1;
}

/*** Conditional branches ***/
/* Branching helper function. Performs branch and calculates cycles */
static void do_branch(CPU* cpu)
{
	/* Extra cycle taken if branch is to another page */
	if ((cpu->oc_addr & 0xFF00) != (cpu->pc & 0xFF00))
		++cpu->cycles;

	/* Perform branch operation */
	cpu->pc = cpu->oc_addr;
	++cpu->cycles;
}

/* BCC - branch if carry flag clear */
static void bcc(CPU* cpu)
{
	if (!(cpu->p & FLAG_C))
		do_branch(cpu);
}

/* BCS - branch if carry flag set */
static void bcs(CPU* cpu)
{
	if (cpu->p & FLAG_C)
		do_branch(cpu);
}

/* BEQ - branch if zero flag set */
static void beq(CPU* cpu)
{
	if (cpu->p & FLAG_Z)
		do_branch(cpu);
}

/* BMI - branch if negative flag set */
static void bmi(CPU* cpu)
{
	if (cpu->p & FLAG_N)
		do_branch(cpu);
}

/* BNE - branch if zero flag clear */
static void bne(CPU* cpu)
{
	if (!(cpu->p & FLAG_Z))
		do_branch(cpu);
}

/* BPL - branch if negative flag clear */
static void bpl(CPU* cpu)
{
	if (!(cpu->p & FLAG_N))
		do_branch(cpu);
}

/* BVC - branch if overflow flag clear */
static void bvc(CPU* cpu)
{
	if (!(cpu->p & FLAG_V))
		do_branch(cpu);
}

/* BVC - branch if overflow flag set */
static void bvs(CPU* cpu)
{
	if (cpu->p & FLAG_V)
		do_branch(cpu);
}

/*** System instructions ***/
/* Jump to interrupt vector. Common behavior to NMI, IRQ, and BRK */
static void jump_interrupt(CPU* cpu, uint16_t vector) {
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc) >> 8);
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc) & 0xFF);
	php(cpu);
	sei(cpu);
	cpu->pc = memory_get16(cpu->nes, vector);
}

/* BRK - software interrupt */
static void brk(CPU* cpu)
{
	/*TODO: brk apparently (sort of) HAS AN OPERAND (second byte is padding)*/
	jump_interrupt(cpu, ADDR_IRQ);
}

/* NOP - no operation */
static void nop(CPU* cpu) {}

/* RTI - return from interrupt */
static void rti(CPU* cpu)
{
	plp(cpu);
	cpu->p &= ~FLAG_B; /* B flag only set on stack */
	cpu->pc = (memory_get(cpu->nes, 0x100 | ++cpu->sp) |
			  (memory_get(cpu->nes, 0x100 | ++cpu->sp) << 8));
}


/*** TODO: Undocumented instructions ***/

/* KIL - crash the processor */
static void kil(CPU* cpu)
{
	/* TODO: CRASH PROCESSOR */
}

/*** Lookup tables ***/
static char* instr_names[256] =
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

static void (*instructions[256])(CPU* cpu) =
{
	brk, ora, kil, nop, nop, ora, asl, nop, php, ora, asl, nop, nop, ora, asl, nop,
	bpl, ora, kil, nop, nop, ora, asl, nop, clc, ora, nop, nop, nop, ora, asl, nop,
	jsr, and, kil, nop, bit, and, rol, nop, plp, and, rol, nop, bit, and, rol, nop,
	bmi, and, kil, nop, nop, and, rol, nop, sec, and, nop, nop, nop, and, rol, nop,
	rti, eor, kil, nop, nop, eor, lsr, nop, pha, eor, lsr, nop, jmp, eor, lsr, nop,
	bvc, eor, kil, nop, nop, eor, lsr, nop, cli, eor, nop, nop, nop, eor, lsr, nop,
	rts, adc, kil, nop, nop, adc, ror, nop, pla, adc, ror, nop, jmp, adc, ror, nop,
	bvs, adc, kil, nop, nop, adc, ror, nop, sei, adc, nop, nop, nop, adc, ror, nop,
	nop, sta, nop, nop, sty, sta, stx, nop, dey, nop, txa, nop, sty, sta, stx, nop,
	bcc, sta, kil, nop, sty, sta, stx, nop, tya, sta, txs, nop, nop, sta, nop, nop,
	ldy, lda, ldx, nop, ldy, lda, ldx, nop, tay, lda, tax, nop, ldy, lda, ldx, nop,
	bcs, lda, kil, nop, ldy, lda, ldx, nop, clv, lda, tsx, nop, ldy, lda, ldx, nop,
	cpy, cmp, nop, nop, cpy, cmp, dec, nop, iny, cmp, dex, nop, cpy, cmp, dec, nop,
	bne, cmp, kil, nop, nop, cmp, dec, nop, cld, cmp, nop, nop, nop, cmp, dec, nop,
	cpx, sbc, nop, nop, cpx, sbc, inc, nop, inx, sbc, nop, nop, cpx, sbc, inc, nop,
	beq, sbc, kil, nop, nop, sbc, inc, nop, sed, sbc, nop, nop, nop, sbc, inc, nop
};

static AddressingMode amodes[256] =
{
	AMODE_IMP, AMODE_XID, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ZPG, AMODE_ZPG, AMODE_NONE,
	AMODE_IMP, AMODE_IMM, AMODE_ACC, AMODE_NONE, AMODE_NONE, AMODE_ABS, AMODE_ABS, AMODE_NONE,
	AMODE_REL, AMODE_IDY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ZPX, AMODE_ZPX, AMODE_NONE,
	AMODE_IMP, AMODE_ABY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ABX, AMODE_ABX, AMODE_NONE,
	AMODE_ABS, AMODE_XID, AMODE_NONE, AMODE_NONE, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_NONE,
	AMODE_IMP, AMODE_IMM, AMODE_ACC, AMODE_NONE, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_NONE,
	AMODE_REL, AMODE_IDY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ZPX, AMODE_ZPX, AMODE_NONE,
	AMODE_IMP, AMODE_ABY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ABX, AMODE_ABX, AMODE_NONE,
	AMODE_IMP, AMODE_XID, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ZPG, AMODE_ZPG, AMODE_NONE,
	AMODE_IMP, AMODE_IMM, AMODE_ACC, AMODE_NONE, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_NONE,
	AMODE_REL, AMODE_IDY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ZPX, AMODE_ZPX, AMODE_NONE,
	AMODE_IMP, AMODE_ABY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ABX, AMODE_ABX, AMODE_NONE,
	AMODE_IMP, AMODE_XID, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ZPG, AMODE_ZPG, AMODE_NONE,
	AMODE_IMP, AMODE_IMM, AMODE_ACC, AMODE_NONE, AMODE_IND, AMODE_ABS, AMODE_ABS, AMODE_NONE,
	AMODE_REL, AMODE_IDY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ZPX, AMODE_ZPX, AMODE_NONE,
	AMODE_IMP, AMODE_ABY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ABX, AMODE_ABX, AMODE_NONE,
	AMODE_NONE, AMODE_XID, AMODE_NONE, AMODE_NONE, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_NONE,
	AMODE_IMP, AMODE_NONE, AMODE_IMP, AMODE_NONE, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_NONE,
	AMODE_REL, AMODE_IDY, AMODE_NONE, AMODE_NONE, AMODE_ZPX, AMODE_ZPX, AMODE_ZPY, AMODE_NONE,
	AMODE_IMP, AMODE_ABY, AMODE_IMP, AMODE_NONE, AMODE_NONE, AMODE_ABX, AMODE_NONE, AMODE_NONE,
	AMODE_IMM, AMODE_XID, AMODE_IMM, AMODE_NONE, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_NONE,
	AMODE_IMP, AMODE_IMM, AMODE_IMP, AMODE_NONE, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_NONE,
	AMODE_REL, AMODE_IDY, AMODE_NONE, AMODE_NONE, AMODE_ZPX, AMODE_ZPX, AMODE_ZPY, AMODE_NONE,
	AMODE_IMP, AMODE_ABY, AMODE_IMP, AMODE_NONE, AMODE_ABX, AMODE_ABX, AMODE_ABY, AMODE_NONE,
	AMODE_IMM, AMODE_XID, AMODE_NONE, AMODE_NONE, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_NONE,
	AMODE_IMP, AMODE_IMM, AMODE_IMP, AMODE_NONE, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_NONE,
	AMODE_REL, AMODE_IDY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ZPX, AMODE_ZPX, AMODE_NONE,
	AMODE_IMP, AMODE_ABY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ABX, AMODE_ABX, AMODE_NONE,
	AMODE_IMM, AMODE_XID, AMODE_NONE, AMODE_NONE, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_NONE,
	AMODE_IMP, AMODE_IMM, AMODE_IMP, AMODE_NONE, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_NONE,
	AMODE_REL, AMODE_IDY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ZPX, AMODE_ZPX, AMODE_NONE,
	AMODE_IMP, AMODE_ABY, AMODE_NONE, AMODE_NONE, AMODE_NONE, AMODE_ABX, AMODE_ABX, AMODE_NONE
};

static uint8_t instruction_cycles[256] =
{
	7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
	6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
	6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
	6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
	2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
	2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
	2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
	2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
	2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
	2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7
};

static uint8_t pagecross_cycles[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0
};

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
	/*cpu->p = 0x20 | FLAG_B | FLAG_I; /* unused, break, and IRQ disable flags *///
	cpu->p = 0x20 | FLAG_I;
	cpu->a = cpu->x = cpu->y = 0;
	cpu->sp = 0;

	/* TODO: don't actually push anything? */
	/* push pc */
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc) >> 8);
	memory_set(cpu->nes, 0x100 | cpu->sp--, (cpu->pc) & 0xFF);

	/* push p */
	php(cpu);

	/* sp should now be 0xFD */

	/* jump to reset vector */
	cpu->pc = memory_get16(cpu->nes, ADDR_RESET);
	/*cpu->pc = 0xC000; /* for nestest */

	/* TODO: init memory */
	/* after reset, IRQ disable flag set to true (ORed with 0x04) */
	/* after reset, APU is silenced */
}

static void handle_interrupt(CPU* cpu, uint8_t type)
{
	if (type == INT_NONE)
		return;

	/* Set jump address to interrupt handler */
	switch (type)
	{
		case INT_RESET:
			cpu_reset(cpu);
			break;
		case INT_NMI:
			jump_interrupt(cpu, ADDR_NMI);
			cpu->cycles += 7;
			break;
		case INT_IRQ:
			/* Don't handle interrupt if IRQs are masked */
			if (!(cpu->p & FLAG_I)) {
				jump_interrupt(cpu, ADDR_IRQ);
				cpu->cycles += 7;
			}
	}

	cpu->interrupt = INT_NONE;
}


/* Absolute indexed addressing helper function. Use the the next two bytes plus
   the value passed (from the appropriate register) to get effective address */
static void get_abs_indexed_oc_addr(CPU* cpu, uint8_t i)
{
	uint16_t addr = memory_get16(cpu->nes, cpu->pc++);
	++cpu->pc; /* for fecthing second byte */
	cpu->oc_addr = addr + i;

	/* Page cross cycle penalty */
	if ((addr & 0xFF00) != (cpu->oc_addr & 0xFF00))
		cpu->cycles += pagecross_cycles[cpu->opcode];
}

static void get_oc_address(CPU* cpu)
{
	switch (cpu->instruction_amode)
	{
		/* Instructions with these addressing modes don't get values from memory */
		case AMODE_NONE:
		case AMODE_ACC:
		case AMODE_IMP:
			cpu->oc_addr = 0;
			break;

		/* Immediate addressing => Next byte is value */
		case AMODE_IMM:
			cpu->oc_addr = cpu->pc++;
			break;

		/* Zero page addressing => Value of next byte equals address */
		case AMODE_ZPG:
			cpu->oc_addr = memory_get(cpu->nes, cpu->pc++) & 0xFF;
			break;

		/* Zero page, X addressing => Value of next byte plus the
		   value in X equals address */
		case AMODE_ZPX:
			cpu->oc_addr = (memory_get(cpu->nes, cpu->pc++) + cpu->x) & 0xFF;
			break;

		/* Zero page, Y addressing => Value of next byte plus the value
		   in Y equals address */
		case AMODE_ZPY:
			cpu->oc_addr = (memory_get(cpu->nes, cpu->pc++) + cpu->y) & 0xFF;
			break;

		/* Relative addressing => PC plus the value of the next byte equals address */
		case AMODE_REL:
		{
			cpu->oc_addr = (int8_t)memory_get(cpu->nes, cpu->pc++) + cpu->pc;
			break;
		}

		/* Absolute addressing => Value of next two bytes equal address */
		case AMODE_ABS:
			cpu->oc_addr = memory_get16(cpu->nes, cpu->pc++);
			++cpu->pc; /* for fetching second byte */
			break;

		/* Absolute, X addressing => Value of next two bytes plus value in X
		   equals address */
		case AMODE_ABX:
			get_abs_indexed_oc_addr(cpu, cpu->x);
			break;

		/* Absolute, X addressing => Value of next two bytes plus value in Y
		   equals address */
		case AMODE_ABY:
			get_abs_indexed_oc_addr(cpu, cpu->y);
			break;

		/* Indirect addressing => Address pointed to by the next 2 bytes equals address */
		case AMODE_IND:
			cpu->oc_addr = memory_get16_ind(cpu->nes, memory_get16(cpu->nes, cpu->pc++));
			++cpu->pc; /* for fetching pointer high byte */
			break;

		/* Indexed indirect addressing => Address pointed to by X plus value of next byte
		   equals address */
		case AMODE_XID:
		{
			uint8_t ptr = (memory_get(cpu->nes, cpu->pc++) + cpu->x) & 0xFF;
			cpu->oc_addr = memory_get16_ind(cpu->nes, ptr);
			break;
		}

		/* Indirect indexed addressing => Y plus address pointed to by value of next two bytes
		   equals address */
		case AMODE_IDY:
		{
			uint16_t ptr = memory_get16_ind(cpu->nes, memory_get(cpu->nes, cpu->pc++) & 0xFF);
			cpu->oc_addr = ptr + cpu->y;

			/* Page cross cycle penalty */
			if ((ptr & 0xFF00) != (cpu->oc_addr & 0xFF00))
				cpu->cycles += pagecross_cycles[cpu->opcode];
			break;
		}
	}
}

#include <stdio.h>
int cpu_step(CPU* cpu)
{
	static int occount = 0;
	/*static FILE* log;*/

	cpu->cycles = 0;

	if (cpu->dma_cycles)
	{
		--cpu->dma_cycles;
		return 1;
	}

	if (cpu->interrupt != INT_NONE) {
		handle_interrupt(cpu, cpu->interrupt);
		return cpu->cycles;
	}

	/*log = fopen("cpu.log", "a");*/

	cpu->opcode = memory_get(cpu->nes, cpu->pc++);
	cpu->instruction_amode = amodes[cpu->opcode];
	/*printf("%X\t%s ", cpu->pc-1, instr_names[cpu->opcode]);
	/*fprintf(log, "%X\t%s ", cpu->pc-1, instr_names[cpu->opcode]);*/
	get_oc_address(cpu);
	/*fprintf(log, "PC:%x\tOPCODE:%s\tOPERAND:%x\tBYTES:%x\n", cpu->pc, instr_names[oci.opcode], oci.operand, instruction_cycles[oci.opcode]);*/
	/*fprintf(log, "$%.4x\t\t\t", cpu->oc_addr);*/
	/*printf("$%.4x\t\t\t", cpu->oc_addr);
	printf("A:%.2X X:%.2X Y:%.2X P:%.2X SP:%.2X\n", cpu->a, cpu->x, cpu->y, cpu->p, cpu->sp);

	/*fprintf(log, "A:%.2X X:%.2X Y:%.2X P:%.2X SP:%.2X\n", cpu->a, cpu->x, cpu->y, cpu->p, cpu->sp);
	fclose(log);*/

	cpu->cycles = instruction_cycles[cpu->opcode];

	/* Used for adding another cycle during DMA writes */
	cpu->oddcycle = (cpu->oddcycle ^ cpu->cycles%2);

	instructions[cpu->opcode](cpu);
	return cpu->cycles;
}

void cpu_interrupt(CPU* cpu, Interrupt type)
{
	cpu->interrupt = type;
}
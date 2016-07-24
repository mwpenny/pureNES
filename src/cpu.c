#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "nes.h"
#include "memory.h"

/* Interrupt vectors */
#define ADDR_NMI 0xFFFA
#define ADDR_RESET 0xFFFC
#define ADDR_IRQ 0xFFFE

/* P flag register bitmasks */
/* 7  bit  0
   ---- ----
   NVUB DIZC
   |||| ||||
   |||| |||+- Carry: 1 if last addition or shift resulted in a carry, or if
   |||| |||     last subtraction resulted in no borrow
   |||| ||+-- Zero: 1 if last operation resulted in a 0 value
   |||| |+--- Interrupt: Interrupt inhibit
   |||| |       (0: /IRQ and /NMI get through; 1: only /NMI gets through)
   |||| +---- Decimal: 1 to make ADC and SBC use binary-coded decimal arithmetic
   ||||         (ignored on second-source 6502 like that in the NES)
   |||+------ Break: Doesn't exist in hardware. 1 when using PHP/BRK (software interrupt)
   ||+------- Unused: Always 1
   |+-------- Overflow: 1 if last ADC or SBC resulted in signed overflow,
   |            or D6 from last BIT
   +--------- Negative: Set to bit 7 of the last operation */
#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

static void update_flags(CPU* cpu, uint8_t val)
{
	/* Set/clear zero and negative flags based on result of last operation */
	if (val)
		cpu->p &= ~FLAG_Z;
	else
		cpu->p |= FLAG_Z;

	if (val & 0x80)
		cpu->p |= FLAG_N;
	else
		cpu->p &= ~FLAG_N;
}

static void stack_push(CPU* cpu, uint8_t val)
{
	memory_set(cpu->nes, 0x100 | cpu->sp--, val);
}

static void stack_push16(CPU* cpu, uint16_t val)
{
	stack_push(cpu, (uint8_t)(val >> 8));
	stack_push(cpu, (uint8_t)(val & 0xFF));
}

static uint8_t stack_pop(CPU* cpu)
{
	return memory_get(cpu->nes, 0x100 | ++cpu->sp);
}

static uint16_t stack_pop16(CPU* cpu)
{
	return stack_pop(cpu) | (stack_pop(cpu) << 8);
}

/*** CPU instructions ***/

/** Status flag changes **/
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

/** Load/store operations **/
/* LDA - load accumulator */
static void lda(CPU* cpu)
{
	cpu->a = memory_get(cpu->nes, cpu->eff_addr);
	update_flags(cpu, cpu->a);
}

/* LDX - load X register */
static void ldx(CPU* cpu)
{
	cpu->x = memory_get(cpu->nes, cpu->eff_addr);
	update_flags(cpu, cpu->x);
}

/* LDY - load Y register */
static void ldy(CPU* cpu)
{
	cpu->y = memory_get(cpu->nes, cpu->eff_addr);
	update_flags(cpu, cpu->y);
}

/* STA - store accumulator */
static void sta(CPU* cpu)
{
	memory_set(cpu->nes, cpu->eff_addr, cpu->a);
}

/* STX - store X register */
static void stx(CPU* cpu)
{
	memory_set(cpu->nes, cpu->eff_addr, cpu->x);
}

/* STY - store Y register */
static void sty(CPU* cpu)
{
	memory_set(cpu->nes, cpu->eff_addr, cpu->y);
}

/** Register transfers **/
/* TAX - transfer accumulator to X */
static void tax(CPU* cpu)
{
	cpu->x = cpu->a;
	update_flags(cpu, cpu->x);
}

/* TAY - transfer accumulator to Y */
static void tay(CPU* cpu)
{
	cpu->y = cpu->a;
	update_flags(cpu, cpu->y);
}

/* TXA - transfer X to accumulator */
static void txa(CPU* cpu)
{
	cpu->a = cpu->x;
	update_flags(cpu, cpu->a);
}

/* TYA - transfer Y to accumulator */
static void tya(CPU* cpu)
{
	cpu->a = cpu->y;
	update_flags(cpu, cpu->a);
}

/** Stack operations **/
/* TSX - transfer stack pointer to X */
static void tsx(CPU* cpu)
{
	cpu->x = cpu->sp;
	update_flags(cpu, cpu->x);
}

/* TXS - transfer X to stack pointer */
static void txs(CPU* cpu)
{
	cpu->sp = cpu->x;
}

/* PHA - push accumulator on stack */
static void pha(CPU* cpu)
{
	stack_push(cpu, cpu->a);
}

/* PHP - push processor status on stack */
static void php(CPU* cpu)
{
	/* B flag set on PHP/BRK (software interrupts) */
	stack_push(cpu, cpu->p | FLAG_B | FLAG_U);
}

/* PLA - pull accumulator from stack */
static void pla(CPU* cpu)
{
	cpu->a = stack_pop(cpu);
	update_flags(cpu, cpu->a);
}

/* PLP - pull processor status from stack */
static void plp(CPU* cpu)
{
	/* B flag doesn't exist in hardware. Don't save to register */
	cpu->p = stack_pop(cpu) & ~FLAG_B;
}

/** Logical operations **/
/* AND - logical AND with accumulator */
static void and(CPU* cpu)
{
	cpu->a &= memory_get(cpu->nes, cpu->eff_addr);
	update_flags(cpu, cpu->a);
}

/* EOR - exclusive OR with accumulator */
static void eor(CPU* cpu)
{
	cpu->a ^= memory_get(cpu->nes, cpu->eff_addr);
	update_flags(cpu, cpu->a);
}

/* ORA - inclusive OR with accumulator */
static void ora(CPU* cpu)
{
	cpu->a |= memory_get(cpu->nes, cpu->eff_addr);
	update_flags(cpu, cpu->a);
}

/* BIT - bit test with mask pattern in accumulator */
static void bit(CPU* cpu)
{
	uint8_t num = memory_get(cpu->nes, cpu->eff_addr);

	/* Clear zero flag if register A has a mask bit set */
	if (cpu->a & num)
		cpu->p &= ~FLAG_Z;
	else
		cpu->p |= FLAG_Z;

	/* Copy N and V bits from the memory location */
	cpu->p = (cpu->p & ~FLAG_N & ~FLAG_V) | (num & (FLAG_N | FLAG_V));
}

/** Arithmetic operations **/
/* Addition helper function. Works with a number directly */
static void do_add(CPU* cpu, uint8_t num)
{
	int16_t val = cpu->a + num + (cpu->p & FLAG_C);
	if (val > 0xFF)
		cpu->p |= FLAG_C;
	else
		cpu->p &= ~FLAG_C;

	/* Sum of 2 numbers with same sign has different sign => overflow */
	if ((~(cpu->a ^ num) & (cpu->a ^ val)) & 0x80)
		cpu->p |= FLAG_V;
	else
		cpu->p &= ~FLAG_V;

	cpu->a = (uint8_t)val;
	update_flags(cpu, cpu->a);
}

/* ADC - add with carry to accumulator */
static void adc(CPU* cpu)
{
	uint8_t num = memory_get(cpu->nes, cpu->eff_addr);
	do_add(cpu, num);
}

/* SBC - subtract with carry from accumulator */
static void sbc(CPU* cpu)
{
	/* -num - 1 */
	uint8_t num = ~memory_get(cpu->nes, cpu->eff_addr);
	do_add(cpu, num);
}

static void do_compare(CPU* cpu, uint8_t reg)
{
	uint8_t num = memory_get(cpu->nes, cpu->eff_addr);
	if (num > reg)
		cpu->p &= ~FLAG_C;
	else
		cpu->p |= FLAG_C;
	update_flags(cpu, reg - num);
}

/* CMP - compare with accumulator */
static void cmp(CPU* cpu)
{
	do_compare(cpu, cpu->a);
}

/* CPX - compare with X register */
static void cpx(CPU* cpu)
{
	do_compare(cpu, cpu->x);
}

/* CPY - compare with Y register */
static void cpy(CPU* cpu)
{
	do_compare(cpu, cpu->y);
}

/** Increments and decrements **/
/* INC - increment value at memory location */
static void inc(CPU* cpu)
{
	uint8_t val = memory_get(cpu->nes, cpu->eff_addr)+1;
	memory_set(cpu->nes, cpu->eff_addr, val);
	update_flags(cpu, val);
}

/* INX - increment X register */
static void inx(CPU* cpu)
{
	++cpu->x;
	update_flags(cpu, cpu->x);
}

/* INY - increment Y register */
static void iny(CPU* cpu)
{
	++cpu->y;
	update_flags(cpu, cpu->y);
}

/* DEC - decrement memory location */
static void dec(CPU* cpu)
{
	uint8_t val = memory_get(cpu->nes, cpu->eff_addr)-1;
	memory_set(cpu->nes, cpu->eff_addr, val);
	update_flags(cpu, val);
}

/* DEX - decrement X register */
static void dex(CPU* cpu)
{
	--cpu->x;
	update_flags(cpu, cpu->x);
}

/* DEY - decrement Y register */
static void dey(CPU* cpu)
{
	--cpu->y;
	update_flags(cpu, cpu->y);
}

/** Shifts **/
static uint8_t get_shift_num(CPU* cpu)
{
	if (cpu->instr_amode == AMODE_ACC)
		return cpu->a;
	else
		return memory_get(cpu->nes, cpu->eff_addr);
}

static void store_shift_result(CPU* cpu, uint8_t num)
{
	if (cpu->instr_amode == AMODE_ACC)
		cpu->a = num;
	else
		memory_set(cpu->nes, cpu->eff_addr, num);
}

/* ASL - arithmetic shift left */
static void asl(CPU* cpu)
{
	/* C <- num <- 0 */
	uint8_t num = get_shift_num(cpu);
	cpu->p = (cpu->p & ~FLAG_C) | ((num >> 7) & 0x01);
	num <<= 1;
	update_flags(cpu, num);
	store_shift_result(cpu, num);
}

/* LSR - logical shift right */
static void lsr(CPU* cpu)
{
	/* 0 -> num -> C */
	uint8_t num = get_shift_num(cpu);
	cpu->p = (cpu->p & ~FLAG_C) | (num & 0x01);
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
	cpu->p = (cpu->p & ~FLAG_C) | ((num >> 7) & 0x01);
	num = (num << 1) | carry;
	update_flags(cpu, num);
	store_shift_result(cpu, num);
}

/* ROR - rotate right */
static void ror(CPU* cpu)
{
	/* C -> num -> C */
	uint8_t num = get_shift_num(cpu);
	uint8_t carry = cpu->p & FLAG_C;
	cpu->p = (cpu->p & ~FLAG_C) | (num & FLAG_C);
	num = (carry << 7) | (num >> 1);
	update_flags(cpu, num);
	store_shift_result(cpu, num);
}

/** Jumps and calls **/
/* JMP - jump to location */
static void jmp(CPU* cpu)
{
	cpu->pc = cpu->eff_addr;
}

/* JSR - jump to subroutine */
static void jsr(CPU* cpu)
{
	/* Push PC onto the stack */
	stack_push16(cpu, cpu->pc-1);
	cpu->pc = cpu->eff_addr;
}

/* RTS - return from subroutine */
static void rts(CPU* cpu)
{
	/* Pull PC off the stack */
	cpu->pc = stack_pop16(cpu) + 1;
}

/** Conditional branches **/
static void do_branch(CPU* cpu)
{
	/* Extra cycle taken if branch is to another page */
	if ((cpu->eff_addr & 0xFF00) != (cpu->pc & 0xFF00))
		++cpu->cycles;

	/* Perform branch operation */
	cpu->pc = cpu->eff_addr;
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

/** System instructions **/
static void jump_interrupt(CPU* cpu, uint16_t vector)
{
	stack_push16(cpu, cpu->pc);
	php(cpu);
	cpu->p |= FLAG_I;
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
	cpu->pc = stack_pop16(cpu);
}

/** Illegal instructions **/
/* TODO: warn if these are used!! */
/* AHX - store A & X & (addr high byte plus 1) at addr */
static void ahx(CPU* cpu)
{
	memory_set(cpu->nes, cpu->eff_addr, cpu->a & cpu->x & ((cpu->eff_addr >> 8) + 1));
}

/* ALR - AND + LSR */
static void alr(CPU* cpu)
{
	and(cpu);
	lsr(cpu);
}

/* ANC - AND + set carry bit if result is negative */
static void anc(CPU* cpu)
{
	and(cpu);
	if (cpu->a & 0x80)
		cpu->p |= FLAG_C;
	else
		cpu->p &= ~FLAG_C;
}

/* ARR - similar to AND + ROR. AND byte with A, V = A.5 XOR A.6,
   swap C and A.6, then finally shift A right (bit 0 is lost) */
static void arr(CPU* cpu)
{
	uint8_t xor, b6;
	uint8_t carry = cpu->p & FLAG_C;
	cpu->a &= memory_get(cpu->nes, cpu->eff_addr);

	b6 = (cpu->a >> 6) & 0x01;
	xor = (b6 ^ (cpu->a >> 5)) & 0x01;
	cpu->p = (cpu->p & ~FLAG_V & ~FLAG_C) | (xor << 6) | b6;
	cpu->a = (carry << 7) | (cpu->a >> 1);
	update_flags(cpu, cpu->a);
}

/* AXS - subtract byte from A & X and store in X */
static void axs(CPU* cpu)
{
	uint8_t num = (cpu->a & cpu->x);
	cpu->x = num - memory_get(cpu->nes, cpu->eff_addr);
	do_compare(cpu, num);
}

/* DCP - DEC + CMP */
static void dcp(CPU* cpu)
{
	dec(cpu);
	cmp(cpu);
}

/* ISC - INC + SBC */
static void isc(CPU* cpu)
{
	inc(cpu);
	sbc(cpu);
}

/* KIL - crash the processor */
static void kil(CPU* cpu)
{
	cpu->is_running = 0;
}

/* LAS - A = X = SP = value & SP */
static void las(CPU* cpu)
{
	uint8_t val = memory_get(cpu->nes, cpu->eff_addr) & cpu->sp;
	cpu->a = cpu->x = cpu->sp = val;
	update_flags(cpu, val);
}

/* LAX - LDA + LDX */
static void lax(CPU* cpu)
{
	lda(cpu);
	ldx(cpu);
}

/* RLA - ROL + AND */
static void rla(CPU* cpu)
{
	rol(cpu);
	and(cpu);
}

/* RRA - ROR + ADC */
static void rra(CPU* cpu)
{
	ror(cpu);
	adc(cpu);
}

/* SAX - store A & X at memory location */
static void sax(CPU* cpu)
{
	memory_set(cpu->nes, cpu->eff_addr, cpu->a & cpu->x);
}

/* SHX - store X & (addr high byte plus 1) at addr */
static void shx(CPU* cpu)
{
	memory_set(cpu->nes, cpu->eff_addr, cpu->x & ((cpu->eff_addr >> 8) + 1));
}

/* SHY - store Y & (addr high byte plus 1) at addr */
static void shy(CPU* cpu)
{
	memory_set(cpu->nes, cpu->eff_addr, cpu->y & ((cpu->eff_addr >> 8) + 1));
}

/* SLO - ASL + ORA */
static void slo(CPU* cpu)
{
	asl(cpu);
	ora(cpu);
}

/* SRE - LSR + EOR */
static void sre(CPU* cpu)
{
	lsr(cpu);
	eor(cpu);
}

/* TAS - store A & X in SP, store result & (addr high byte plus 1) at addr */
static void tas(CPU* cpu)
{
	cpu->sp = cpu->a & cpu->x;
	memory_set(cpu->nes, cpu->eff_addr, cpu->sp & ((cpu->eff_addr >> 8) + 1));
}

/* XAA - TXA + AND */
void xaa(CPU* cpu)
{
	/* TODO: highly unstable (use magic) */
	txa(cpu);
	and(cpu);
}

/*** Lookup tables ***/
static char* instr_names[256] =
{
	"BRK", "ORA", "KIL", "SLO", "NOP", "ORA", "ASL", "SLO",
	"PHP", "ORA", "ASL", "ANC", "NOP", "ORA", "ASL", "SLO",
	"BPL", "ORA", "KIL", "SLO", "NOP", "ORA", "ASL", "SLO",
	"CLC", "ORA", "NOP", "SLO", "NOP", "ORA", "ASL", "SLO",
	"JSR", "AND", "KIL", "RLA", "BIT", "AND", "ROL", "RLA",
	"PLP", "AND", "ROL", "ANC", "BIT", "AND", "ROL", "RLA",
	"BMI", "AND", "KIL", "RLA", "NOP", "AND", "ROL", "RLA",
	"SEC", "AND", "NOP", "RLA", "NOP", "AND", "ROL", "RLA",
	"RTI", "EOR", "KIL", "SRE", "NOP", "EOR", "LSR", "SRE",
	"PHA", "EOR", "LSR", "ALR", "JMP", "EOR", "LSR", "SRE",
	"BVC", "EOR", "KIL", "SRE", "NOP", "EOR", "LSR", "SRE",
	"CLI", "EOR", "NOP", "SRE", "NOP", "EOR", "LSR", "SRE",
	"RTS", "ADC", "KIL", "RRA", "NOP", "ADC", "ROR", "RRA",
	"PLA", "ADC", "ROR", "ARR", "JMP", "ADC", "ROR", "RRA",
	"BVS", "ADC", "KIL", "RRA", "NOP", "ADC", "ROR", "RRA",
	"SEI", "ADC", "NOP", "RRA", "NOP", "ADC", "ROR", "RRA",
	"NOP", "STA", "NOP", "SAX", "STY", "STA", "STX", "SAX",
	"DEY", "NOP", "TXA", "XAA", "STY", "STA", "STX", "SAX",
	"BCC", "STA", "KIL", "AHX", "STY", "STA", "STX", "SAX",
	"TYA", "STA", "TXS", "TAS", "NOP", "STA", "NOP", "AHX",
	"LDY", "LDA", "LDX", "LAX", "LDY", "LDA", "LDX", "LAX",
	"TAY", "LDA", "TAX", "LAX", "LDY", "LDA", "LDX", "LAX",
	"BCS", "LDA", "KIL", "LAX", "LDY", "LDA", "LDX", "LAX",
	"CLV", "LDA", "TSX", "LAS", "LDY", "LDA", "LDX", "LAX",
	"CPY", "CMP", "NOP", "DCP", "CPY", "CMP", "DEC", "DCP",
	"INY", "CMP", "DEX", "AXS", "CPY", "CMP", "DEC", "DCP",
	"BNE", "CMP", "KIL", "DCP", "NOP", "CMP", "DEC", "DCP",
	"CLD", "CMP", "NOP", "DCP", "NOP", "CMP", "DEC", "DCP",
	"CPX", "SBC", "NOP", "ISC", "CPX", "SBC", "INC", "ISC",
	"INX", "SBC", "NOP", "SBC", "CPX", "SBC", "INC", "ISC",
	"BEQ", "SBC", "KIL", "ISC", "NOP", "SBC", "INC", "ISC",
	"SED", "SBC", "NOP", "ISC", "NOP", "SBC", "INC", "ISC"
};

static void (*instructions[256])(CPU* cpu) =
{
	brk, ora, kil, slo, nop, ora, asl, slo, php, ora, asl, anc, nop, ora, asl, slo,
	bpl, ora, kil, slo, nop, ora, asl, slo, clc, ora, nop, slo, nop, ora, asl, slo,
	jsr, and, kil, rla, bit, and, rol, rla, plp, and, rol, anc, bit, and, rol, rla,
	bmi, and, kil, rla, nop, and, rol, rla, sec, and, nop, rla, nop, and, rol, rla,
	rti, eor, kil, sre, nop, eor, lsr, sre, pha, eor, lsr, alr, jmp, eor, lsr, sre,
	bvc, eor, kil, sre, nop, eor, lsr, sre, cli, eor, nop, sre, nop, eor, lsr, sre,
	rts, adc, kil, rra, nop, adc, ror, rra, pla, adc, ror, arr, jmp, adc, ror, rra,
	bvs, adc, kil, rra, nop, adc, ror, rra, sei, adc, nop, rra, nop, adc, ror, rra,
	nop, sta, nop, sax, sty, sta, stx, sax, dey, nop, txa, xaa, sty, sta, stx, sax,
	bcc, sta, kil, ahx, sty, sta, stx, sax, tya, sta, txs, tas, shy, sta, shx, ahx,
	ldy, lda, ldx, lax, ldy, lda, ldx, lax, tay, lda, tax, lax, ldy, lda, ldx, lax,
	bcs, lda, kil, lax, ldy, lda, ldx, lax, clv, lda, tsx, las, ldy, lda, ldx, lax,
	cpy, cmp, nop, dcp, cpy, cmp, dec, dcp, iny, cmp, dex, axs, cpy, cmp, dec, dcp,
	bne, cmp, kil, dcp, nop, cmp, dec, dcp, cld, cmp, nop, dcp, nop, cmp, dec, dcp,
	cpx, sbc, nop, isc, cpx, sbc, inc, isc, inx, sbc, nop, sbc, cpx, sbc, inc, isc,
	beq, sbc, kil, isc, nop, sbc, inc, isc, sed, sbc, nop, isc, nop, sbc, inc, isc
};

static AddressingMode amodes[256] =
{
	AMODE_IMP, AMODE_XID, AMODE_IMP, AMODE_XID, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG,
	AMODE_IMP, AMODE_IMM, AMODE_ACC, AMODE_IMM, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_ABS,
	AMODE_REL, AMODE_IDY, AMODE_IMP, AMODE_IDY, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX,
	AMODE_IMP, AMODE_ABY, AMODE_IMP, AMODE_ABY, AMODE_ABX, AMODE_ABX, AMODE_ABX, AMODE_ABX,
	AMODE_ABS, AMODE_XID, AMODE_IMP, AMODE_XID, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG,
	AMODE_IMP, AMODE_IMM, AMODE_ACC, AMODE_IMM, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_ABS,
	AMODE_REL, AMODE_IDY, AMODE_IMP, AMODE_IDY, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX,
	AMODE_IMP, AMODE_ABY, AMODE_IMP, AMODE_ABY, AMODE_ABX, AMODE_ABX, AMODE_ABX, AMODE_ABX,
	AMODE_IMP, AMODE_XID, AMODE_IMP, AMODE_XID, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG,
	AMODE_IMP, AMODE_IMM, AMODE_ACC, AMODE_IMM, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_ABS,
	AMODE_REL, AMODE_IDY, AMODE_IMP, AMODE_IDY, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX,
	AMODE_IMP, AMODE_ABY, AMODE_IMP, AMODE_ABY, AMODE_ABX, AMODE_ABX, AMODE_ABX, AMODE_ABX,
	AMODE_IMP, AMODE_XID, AMODE_IMP, AMODE_XID, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG,
	AMODE_IMP, AMODE_IMM, AMODE_ACC, AMODE_IMM, AMODE_IND, AMODE_ABS, AMODE_ABS, AMODE_ABS,
	AMODE_REL, AMODE_IDY, AMODE_IMP, AMODE_IDY, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX,
	AMODE_IMP, AMODE_ABY, AMODE_IMM, AMODE_ABY, AMODE_ABX, AMODE_ABX, AMODE_ABX, AMODE_ABX,
	AMODE_IMM, AMODE_XID, AMODE_IMM, AMODE_XID, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG,
	AMODE_IMP, AMODE_IMM, AMODE_IMP, AMODE_IMM, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_ABS,
	AMODE_REL, AMODE_IDY, AMODE_IMP, AMODE_IDY, AMODE_ZPX, AMODE_ZPX, AMODE_ZPY, AMODE_ZPY,
	AMODE_IMP, AMODE_ABY, AMODE_IMP, AMODE_ABY, AMODE_ABX, AMODE_ABX, AMODE_ABY, AMODE_ABY,
	AMODE_IMM, AMODE_XID, AMODE_IMM, AMODE_XID, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG,
	AMODE_IMP, AMODE_IMM, AMODE_IMP, AMODE_IMM, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_ABS,
	AMODE_REL, AMODE_IDY, AMODE_IMP, AMODE_IDY, AMODE_ZPX, AMODE_ZPX, AMODE_ZPY, AMODE_ZPY,
	AMODE_IMP, AMODE_ABY, AMODE_IMP, AMODE_ABY, AMODE_ABX, AMODE_ABX, AMODE_ABY, AMODE_ABY,
	AMODE_IMM, AMODE_XID, AMODE_IMM, AMODE_XID, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG,
	AMODE_IMP, AMODE_IMM, AMODE_IMP, AMODE_IMM, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_ABS,
	AMODE_REL, AMODE_IDY, AMODE_IMP, AMODE_IDY, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX,
	AMODE_IMP, AMODE_ABY, AMODE_IMP, AMODE_ABY, AMODE_ABX, AMODE_ABX, AMODE_ABX, AMODE_ABX,
	AMODE_IMM, AMODE_XID, AMODE_IMM, AMODE_XID, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG, AMODE_ZPG,
	AMODE_IMP, AMODE_IMM, AMODE_IMP, AMODE_IMM, AMODE_ABS, AMODE_ABS, AMODE_ABS, AMODE_ABS,
	AMODE_REL, AMODE_IDY, AMODE_IMP, AMODE_IDY, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX, AMODE_ZPX,
	AMODE_IMP, AMODE_ABY, AMODE_IMP, AMODE_ABY, AMODE_ABX, AMODE_ABX, AMODE_ABX, AMODE_ABX
};

static uint8_t instr_cycles[256] =
{
	7, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
	2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
	6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
	2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
	6, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
	2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
	6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,
	2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
	2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
	2, 6, 0, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
	2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
	2, 5, 0, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
	2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
	2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
	2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
	2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7
};

/* TODO: update for illegal ops */
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
	cpu->nes = nes; /* set parent system */
}

void cpu_reset(CPU* cpu)
{
	cpu->p = FLAG_U | FLAG_I;
	cpu->a = cpu->x = cpu->y = 0;
	cpu->sp = 0xFD;
	cpu->pc = memory_get16(cpu->nes, ADDR_RESET);
	cpu->is_running = 1;

	/* TODO: init memory */
	/* after reset, APU is silenced */
}

static void handle_interrupt(CPU* cpu, InterruptType type)
{
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
			break;
	}

	cpu->last_int = INT_NONE;
}

static void update_eff_addr_abs_indexed(CPU* cpu, uint8_t i)
{
	/* Use the the next two bytes plus the value passed to get effective address */
	uint16_t addr = memory_get16(cpu->nes, cpu->pc++);
	++cpu->pc; /* for fecthing second byte */
	cpu->eff_addr = addr + i;

	/* Page cross cycle penalty */
	if ((addr & 0xFF00) != (cpu->eff_addr & 0xFF00))
		cpu->cycles += pagecross_cycles[cpu->opcode];
}

static void update_eff_addr(CPU* cpu)
{
	switch (cpu->instr_amode)
	{
		/* Instructions with these addressing modes don't get values from memory */
		case AMODE_ACC:
		case AMODE_IMP:
			cpu->eff_addr = 0;
			break;

		/* Immediate addressing => Next byte is value */
		case AMODE_IMM:
			cpu->eff_addr = cpu->pc++;
			break;

		/* Zero page addressing => Value of next byte equals address */
		case AMODE_ZPG:
			cpu->eff_addr = memory_get(cpu->nes, cpu->pc++) & 0xFF;
			break;

		/* Zero page, X addressing => Value of next byte plus the
		   value in X equals address */
		case AMODE_ZPX:
			cpu->eff_addr = (memory_get(cpu->nes, cpu->pc++) + cpu->x) & 0xFF;
			break;

		/* Zero page, Y addressing => Value of next byte plus the value
		   in Y equals address */
		case AMODE_ZPY:
			cpu->eff_addr = (memory_get(cpu->nes, cpu->pc++) + cpu->y) & 0xFF;
			break;

		/* Relative addressing => PC plus the value of the next byte equals address */
		case AMODE_REL:
			cpu->eff_addr = (int8_t)memory_get(cpu->nes, cpu->pc++) + cpu->pc;
			break;

		/* Absolute addressing => Value of next two bytes equal address */
		case AMODE_ABS:
			cpu->eff_addr = memory_get16(cpu->nes, cpu->pc++);
			++cpu->pc; /* for fetching second byte */
			break;

		/* Absolute, X addressing => Value of next two bytes plus value in X
		   equals address */
		case AMODE_ABX:
			update_eff_addr_abs_indexed(cpu, cpu->x);
			break;

		/* Absolute, X addressing => Value of next two bytes plus value in Y
		   equals address */
		case AMODE_ABY:
			update_eff_addr_abs_indexed(cpu, cpu->y);
			break;

		/* Indirect addressing => Address pointed to by the next 2 bytes equals address */
		case AMODE_IND:
			cpu->eff_addr = memory_get16_ind(cpu->nes, memory_get16(cpu->nes, cpu->pc++));
			++cpu->pc; /* for fetching pointer high byte */
			break;

		/* Indexed indirect addressing => Address pointed to by X plus value of next byte
		   equals address */
		case AMODE_XID:
		{
			uint8_t ptr = (memory_get(cpu->nes, cpu->pc++) + cpu->x) & 0xFF;
			cpu->eff_addr = memory_get16_ind(cpu->nes, ptr);
			break;
		}

		/* Indirect indexed addressing => Y plus address pointed to by value of next byte */
		case AMODE_IDY:
		{
			uint16_t ptr = memory_get16_ind(cpu->nes, memory_get(cpu->nes, cpu->pc++) & 0xFF);
			cpu->eff_addr = ptr + cpu->y;

			/* Page cross cycle penalty */
			if ((ptr & 0xFF00) != (cpu->eff_addr & 0xFF00))
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

	if (cpu->last_int != INT_NONE) {
		handle_interrupt(cpu, cpu->last_int);
		return cpu->cycles;
	}

	/*log = fopen("cpu.log", "a");*/

	cpu->opcode = memory_get(cpu->nes, cpu->pc++);
	cpu->instr_amode = amodes[cpu->opcode];
	/*printf("%X\t%s ", cpu->pc-1, instr_names[cpu->opcode]);
	/*fprintf(log, "%X\t%s ", cpu->pc-1, instr_names[cpu->opcode]);*/
	update_eff_addr(cpu);
	/*fprintf(log, "PC:%x\tOPCODE:%s\tOPERAND:%x\tBYTES:%x\n", cpu->pc, instr_names[oci.opcode], oci.operand, instr_cycles[oci.opcode]);*/
	/*fprintf(log, "$%.4x\t\t\t", cpu->eff_addr);*/
	/*printf("$%.4x\t\t\t", cpu->eff_addr);
	printf("A:%.2X X:%.2X Y:%.2X P:%.2X SP:%.2X\n", cpu->a, cpu->x, cpu->y, cpu->p, cpu->sp);

	/*fprintf(log, "A:%.2X X:%.2X Y:%.2X P:%.2X SP:%.2X\n", cpu->a, cpu->x, cpu->y, cpu->p, cpu->sp);
	fclose(log);*/

	cpu->cycles = instr_cycles[cpu->opcode];

	/* Used for adding another cycle during DMA writes */
	cpu->oddcycle = (cpu->oddcycle ^ cpu->cycles%2);

	instructions[cpu->opcode](cpu);
	return cpu->cycles;
}

void cpu_interrupt(CPU* cpu, InterruptType type)
{
	cpu->last_int = type;
}
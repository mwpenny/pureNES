/* Ricoh 2A03 CPU interpreter.
   The 2A03's CPU core is essentially a MOS 6502 with BCD mode removed */
#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "memory.h"
#include "nes.h"

/* Interrupt vectors */
#define ADDR_NMI 0xFFFA
#define ADDR_RESET 0xFFFC
#define ADDR_IRQ 0xFFFE

#define STACK_START 0x100

/* P flag register bitmasks */
/* 7  bit  0
   ---- ----
   NVUB DIZC
   |||| ||||
   |||| |||+- Carry: Set if last addition or shift resulted in a carry, or if
   |||| |||          last subtraction resulted in no borrow
   |||| ||+-- Zero: Set if the result of the last operation was 0
   |||| |+--- Interrupt mask: When set, only NMIs are handled
   |||| +---- BCD mode enable: Unused on the 2A03
   |||+------ Break: Set in P byte pushed by PHP/BRK (software interrupt)
   ||+------- Unused: Set when P is pushed to the stack
   |+-------- Overflow: Set if last operation resulted in signed overflow
   +--------- Negative: Set to bit 7 of the last operation */
#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

static void update_zn(CPU* cpu, uint8_t val)
{
	/* Update zero and negative flags based on result of last operation */
	cpu->p = (cpu->p & ~FLAG_Z & ~FLAG_N);
	if (!val)
		cpu->p |= FLAG_Z;
	else if (val & 0x80)
		cpu->p |= FLAG_N;
}

static void stack_push(CPU* cpu, uint8_t val)
{
	/* Stack pointer points to an empty location */
	memory_set(cpu->nes, STACK_START | cpu->sp--, val);
}

static void stack_push16(CPU* cpu, uint16_t val)
{
	/* 6502 is little-endian */
	stack_push(cpu, (uint8_t)(val >> 8));
	stack_push(cpu, (uint8_t)(val & 0xFF));
}

static uint8_t stack_pop(CPU* cpu)
{
	return memory_get(cpu->nes, STACK_START | ++cpu->sp);
}

static uint16_t stack_pop16(CPU* cpu)
{
	return stack_pop(cpu) | (stack_pop(cpu) << 8);
}

/*** CPU instructions ***/

/** Status flag changes **/
static void clc(CPU* cpu)
{
	/* Clear carry flag */
	cpu->p &= ~FLAG_C;
}
static void cld(CPU* cpu)
{
	/* Clear decimal mode flag */
	cpu->p &= ~FLAG_D;
}
static void cli(CPU* cpu)
{
	/* Clear interrupt disable flag */
	cpu->p &= ~FLAG_I;
}
static void clv(CPU* cpu)
{
	/* Clear overflow flag */
	cpu->p &= ~FLAG_V;
}
static void sec(CPU* cpu)
{
	/* Set carry flag */
	cpu->p |= FLAG_C;
}
static void sed(CPU* cpu)
{
	/* Set decimal mode flag */
	cpu->p |= FLAG_D;
}
static void sei(CPU* cpu)
{
	/* Set interrupt disable flag */
	cpu->p |= FLAG_I;
}

/** Load/store operations **/
static void lda(CPU* cpu)
{
	/* Load accumulator */
	cpu->a = memory_get(cpu->nes, cpu->eff_addr);
	update_zn(cpu, cpu->a);
}
static void ldx(CPU* cpu)
{
	/* Load X register */
	cpu->x = memory_get(cpu->nes, cpu->eff_addr);
	update_zn(cpu, cpu->x);
}
static void ldy(CPU* cpu)
{
	/* Load Y register */
	cpu->y = memory_get(cpu->nes, cpu->eff_addr);
	update_zn(cpu, cpu->y);
}
static void sta(CPU* cpu)
{
	/* Store accumulator */
	memory_set(cpu->nes, cpu->eff_addr, cpu->a);
}
static void stx(CPU* cpu)
{
	/* Store X register */
	memory_set(cpu->nes, cpu->eff_addr, cpu->x);
}
static void sty(CPU* cpu)
{
	/* Store Y register */
	memory_set(cpu->nes, cpu->eff_addr, cpu->y);
}

/** Register transfers **/
static void tax(CPU* cpu)
{
	/* Transfer accumulator to X */
	update_zn(cpu, cpu->x = cpu->a);
}
static void tay(CPU* cpu)
{
	/* Transfer accumulator to Y */
	update_zn(cpu, cpu->y = cpu->a);
}
static void txa(CPU* cpu)
{
	/* Transfer X to accumulator */
	update_zn(cpu, cpu->a = cpu->x);
}
static void tya(CPU* cpu)
{
	/* Transfer Y to accumulator */
	update_zn(cpu, cpu->a = cpu->y);
}

/** Stack operations **/
static void tsx(CPU* cpu)
{
	/* Transfer stack pointer to X */
	update_zn(cpu, cpu->x = cpu->sp);
}
static void txs(CPU* cpu)
{
	/* Transfer X to stack pointer */
	cpu->sp = cpu->x;
}
static void pha(CPU* cpu)
{
	/* Push accumulator on stack */
	stack_push(cpu, cpu->a);
}
static void php(CPU* cpu)
{
	/* Push processor status on stack.
	   PHP and BRK also set the U and B flags (indicate SW interrupt) */
	stack_push(cpu, cpu->p | FLAG_B | FLAG_U);
}
static void pla(CPU* cpu)
{
	/* Pull accumulator from stack */
	update_zn(cpu, cpu->a = stack_pop(cpu));
}
static void plp(CPU* cpu)
{
	/* Pull processor status from stack.
	   The B and U flags don't exist in hardware, so they get unset */
	cpu->p = stack_pop(cpu) & ~FLAG_B & ~FLAG_U;
}

/** Bitwise operations **/
static void and(CPU* cpu)
{
	/* Logical AND with accumulator */
	cpu->a &= memory_get(cpu->nes, cpu->eff_addr);
	update_zn(cpu, cpu->a);
}
static void eor(CPU* cpu)
{
	/* Exclusive OR with accumulator */
	cpu->a ^= memory_get(cpu->nes, cpu->eff_addr);
	update_zn(cpu, cpu->a);
}
static void ora(CPU* cpu)
{
	/* Inclusive OR with accumulator */
	cpu->a |= memory_get(cpu->nes, cpu->eff_addr);
	update_zn(cpu, cpu->a);
}
static void bit(CPU* cpu)
{
	/* Bit test:
	   ANDs the memory location with A, setting the zero flag accordingly.
	   N and V bits are copied from the memory location */
	uint8_t val = memory_get(cpu->nes, cpu->eff_addr);
	cpu->p = ((cpu->p & ~FLAG_Z & ~FLAG_N & ~FLAG_V) |
			  (val & (FLAG_N | FLAG_V)));
	if (!(cpu->a & val))
		cpu->p |= FLAG_Z;
}

/** Arithmetic operations **/
static void do_add(CPU* cpu, uint8_t val)
{
	/* Add A register and val and set flags */
	int16_t sum = cpu->a + val + (cpu->p & FLAG_C);
	if (sum > 0xFF)
		cpu->p |= FLAG_C;
	else
		cpu->p &= ~FLAG_C;

	/* Sum has different sign than the input values: overflow has occurred */
	if (((cpu->a ^ sum) & (val ^ sum)) & 0x80)
		cpu->p |= FLAG_V;
	else
		cpu->p &= ~FLAG_V;

	cpu->a = (uint8_t)sum;
	update_zn(cpu, cpu->a);
}
static void adc(CPU* cpu)
{
	/* Add with carry to accumulator */
	do_add(cpu, memory_get(cpu->nes, cpu->eff_addr));
}
static void sbc(CPU* cpu)
{
	/* Subtract with carry from accumulator:
	   ~val = -val - 1
	   Addition of carry bit will acount for the 1 */
	do_add(cpu, ~memory_get(cpu->nes, cpu->eff_addr));
}
static void do_compare(CPU* cpu, uint8_t reg)
{
	uint8_t val = memory_get(cpu->nes, cpu->eff_addr);
	if (val > reg)
		cpu->p &= ~FLAG_C;
	else
		cpu->p |= FLAG_C;
	update_zn(cpu, reg - val);
}
static void cmp(CPU* cpu)
{
	/* Compare with accumulator */
	do_compare(cpu, cpu->a);
}
static void cpx(CPU* cpu)
{
	/* Compare with X register */
	do_compare(cpu, cpu->x);
}
static void cpy(CPU* cpu)
{
	/* Compare with Y register */
	do_compare(cpu, cpu->y);
}
static void inc(CPU* cpu)
{
	/* Increment value at memory location */
	uint8_t val = memory_get(cpu->nes, cpu->eff_addr) + 1;
	memory_set(cpu->nes, cpu->eff_addr, val);
	update_zn(cpu, val);
}
static void inx(CPU* cpu)
{
	/* Increment X register */
	update_zn(cpu, ++cpu->x);
}
static void iny(CPU* cpu)
{
	/* Increment Y register */
	update_zn(cpu, ++cpu->y);
}
static void dec(CPU* cpu)
{
	/* Decrement memory location */
	uint8_t val = memory_get(cpu->nes, cpu->eff_addr) - 1;
	memory_set(cpu->nes, cpu->eff_addr, val);
	update_zn(cpu, val);
}
static void dex(CPU* cpu)
{
	/* Decrement X register */
	update_zn(cpu, --cpu->x);
}
static void dey(CPU* cpu)
{
	/* Decrement Y register */
	update_zn(cpu, --cpu->y);
}

/** Shifts **/
static uint8_t get_shifting_val(CPU* cpu)
{
	if (cpu->instr_amode == AMODE_ACC)
		return cpu->a;
	return memory_get(cpu->nes, cpu->eff_addr);
}
static void store_shift_result(CPU* cpu, uint8_t num)
{
	if (cpu->instr_amode == AMODE_ACC)
		cpu->a = num;
	else
		memory_set(cpu->nes, cpu->eff_addr, num);
}
static void asl(CPU* cpu)
{
	/* Arithmetic shift left:
	   C <- num <- 0 */
	uint8_t num = get_shifting_val(cpu);
	cpu->p = (cpu->p & ~FLAG_C) | ((num >> 7) & 1);
	num <<= 1;
	update_zn(cpu, num);
	store_shift_result(cpu, num);
}
static void lsr(CPU* cpu)
{
	/* Logical shift right:
	   0 -> num -> C */
	uint8_t num = get_shifting_val(cpu);
	cpu->p = (cpu->p & ~FLAG_C) | (num & 1);
	num >>= 1;
	update_zn(cpu, num);
	store_shift_result(cpu, num);
}
static void rol(CPU* cpu)
{
	/* Rotate left:
	   C <- num <- C */
	uint8_t num = get_shifting_val(cpu);
	uint8_t carry = (cpu->p & FLAG_C);
	cpu->p = (cpu->p & ~FLAG_C) | ((num >> 7) & 1);
	num = (num << 1) | carry;
	update_zn(cpu, num);
	store_shift_result(cpu, num);
}
static void ror(CPU* cpu)
{
	/* Rotate right:
	   C -> num -> C */
	uint8_t num = get_shifting_val(cpu);
	uint8_t carry = (cpu->p & FLAG_C);
	cpu->p = (cpu->p & ~FLAG_C) | (num & 1);
	num = (carry << 7) | (num >> 1);
	update_zn(cpu, num);
	store_shift_result(cpu, num);
}

/** Jumps and calls **/
static void jmp(CPU* cpu)
{
	/* Jump to location */
	cpu->pc = cpu->eff_addr;
}
static void jsr(CPU* cpu)
{
	/* Push PC onto the stack and jump to the subroutine */
	stack_push16(cpu, cpu->pc - 1);
	cpu->pc = cpu->eff_addr;
}
static void rts(CPU* cpu)
{
	/* Pull PC off the stack and jump to the return address */
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
static void bcc(CPU* cpu)
{
	/* Branch if carry flag clear */
	if (!(cpu->p & FLAG_C))
		do_branch(cpu);
}
static void bcs(CPU* cpu)
{
	/* Branch if carry flag set */
	if (cpu->p & FLAG_C)
		do_branch(cpu);
}
static void beq(CPU* cpu)
{
	/* Branch if zero flag set */
	if (cpu->p & FLAG_Z)
		do_branch(cpu);
}
static void bmi(CPU* cpu)
{
	/* Branch if negative flag set */
	if (cpu->p & FLAG_N)
		do_branch(cpu);
}
static void bne(CPU* cpu)
{
	/* Branch if zero flag clear */
	if (!(cpu->p & FLAG_Z))
		do_branch(cpu);
}
static void bpl(CPU* cpu)
{
	/* Branch if negative flag clear */
	if (!(cpu->p & FLAG_N))
		do_branch(cpu);
}
static void bvc(CPU* cpu)
{
	/* Branch if overflow flag clear */
	if (!(cpu->p & FLAG_V))
		do_branch(cpu);
}
static void bvs(CPU* cpu)
{
	/* Branch if overflow flag set */
	if (cpu->p & FLAG_V)
		do_branch(cpu);
}

/** System instructions **/
static void jump_interrupt(CPU* cpu, uint16_t vector)
{
	uint8_t flags = cpu->p | FLAG_U;
	if (vector == ADDR_IRQ)
		flags |= FLAG_B;  /* B flag set on SW interrupts */
	stack_push16(cpu, cpu->pc);
	stack_push(cpu, flags);
	cpu->p |= FLAG_I;  /* Mask interrupts while handling this one */
	cpu->pc = memory_get16(cpu->nes, vector);
}
static void brk(CPU* cpu)
{
	/* Software interrupt */
	jump_interrupt(cpu, ADDR_IRQ);
}
static void nop(CPU* cpu) {}
static void rti(CPU* cpu)
{
	/* Return from interrupt */
	plp(cpu);
	cpu->p &= ~FLAG_B;  /* B flag only set on stack */
	cpu->pc = stack_pop16(cpu);
}

/** Illegal instructions **/
static void ahx(CPU* cpu)
{
	/* Store A & X & (addr high byte plus 1) at addr */
	uint8_t val = cpu->a & cpu->x & ((cpu->eff_addr >> 8) + 1);
	memory_set(cpu->nes, cpu->eff_addr, val);
}
static void alr(CPU* cpu)
{
	and(cpu);
	lsr(cpu);
}
static void anc(CPU* cpu)
{
	/* AND + set carry bit if result is negative */
	and(cpu);
	if (cpu->a & 0x80)
		cpu->p |= FLAG_C;
	else
		cpu->p &= ~FLAG_C;
}
static void arr(CPU* cpu)
{
	/* Similar to AND + ROR. AND byte with A, V = A.5 XOR A.6,
       swap C and A.6, then finally shift A right (bit 0 is lost) */
	uint8_t xor, b6;
	uint8_t carry = cpu->p & FLAG_C;

	cpu->a &= memory_get(cpu->nes, cpu->eff_addr);
	b6 = (cpu->a >> 6) & 1;
	xor = (b6 ^ (cpu->a >> 5)) & 1;
	cpu->p = (cpu->p & ~FLAG_V & ~FLAG_C) | (xor << 6) | b6;
	cpu->a = (carry << 7) | (cpu->a >> 1);
	update_zn(cpu, cpu->a);
}
static void axs(CPU* cpu)
{
	/* Subtract byte from A & X and store in X */
	uint8_t val = (cpu->a & cpu->x) - memory_get(cpu->nes, cpu->eff_addr);
	cpu->x = val;
	do_compare(cpu, val);
}
static void dcp(CPU* cpu)
{
	dec(cpu);
	cmp(cpu);
}
static void isc(CPU* cpu)
{
	inc(cpu);
	sbc(cpu);
}
static void kil(CPU* cpu)
{
	/* Crash the processor */
	cpu->is_running = 0;
}
static void las(CPU* cpu)
{
	/* A = X = SP = value & SP */
	uint8_t val = memory_get(cpu->nes, cpu->eff_addr) & cpu->sp;
	cpu->a = cpu->x = cpu->sp = val;
	update_zn(cpu, val);
}
static void lax(CPU* cpu)
{
	lda(cpu);
	ldx(cpu);
}
static void rla(CPU* cpu)
{
	rol(cpu);
	and(cpu);
}
static void rra(CPU* cpu)
{
	ror(cpu);
	adc(cpu);
}
static void sax(CPU* cpu)
{
	/* Store A & X at memory location */
	memory_set(cpu->nes, cpu->eff_addr, cpu->a & cpu->x);
}
static void shx(CPU* cpu)
{
	/* Store X & (addr high byte plus 1) at addr */
	memory_set(cpu->nes, cpu->eff_addr, cpu->x & ((cpu->eff_addr >> 8) + 1));
}
static void shy(CPU* cpu)
{
	/* Store Y & (addr high byte plus 1) at addr */
	memory_set(cpu->nes, cpu->eff_addr, cpu->y & ((cpu->eff_addr >> 8) + 1));
}
static void slo(CPU* cpu)
{
	asl(cpu);
	ora(cpu);
}
static void sre(CPU* cpu)
{
	lsr(cpu);
	eor(cpu);
}
static void tas(CPU* cpu)
{
	/* Store A & X in SP, store result & (addr high byte plus 1) at addr */
	uint8_t val = cpu->a & cpu->x;
	cpu->sp = val;
	memory_set(cpu->nes, cpu->eff_addr, val & ((cpu->eff_addr >> 8) + 1));
}
void xaa(CPU* cpu)
{
	/* Similar to TXA + AND, but unstable due to analog feedback */
	cpu->a = (cpu->a | 0xEE) & cpu->x & memory_get(cpu->nes, cpu->eff_addr);
}

/*static char* instr_names[256] =
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
};*/
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
	cpu->nes = nes;
}

void cpu_reset(CPU* cpu)
{
	cpu->sp -= 3;
	cpu->p |= FLAG_I;
	cpu->pc = memory_get16(cpu->nes, ADDR_RESET);
	cpu->is_running = 1;
}

void cpu_power(CPU* cpu)
{
	cpu->p = FLAG_I;
	cpu->a = cpu->x = cpu->y = 0;
	cpu->sp = 0;
	cpu_reset(cpu);
}

static uint8_t get_pagecross_penalty(CPU* cpu, uint16_t oldaddr)
{
	/* Some instructions incur a cycle penalty if the new offset effective
	   address is on a different page than the old one */
	if ((oldaddr & 0xFF00) != (cpu->eff_addr & 0xFF00))
		return pagecross_cycles[cpu->opcode];
	return 0;
}

static void set_eff_addr_abs_indexed(CPU* cpu, uint8_t val)
{
	/* Use the the next two bytes + the value passed to get effective address */
	uint16_t addr = memory_get16(cpu->nes, cpu->pc++);
	++cpu->pc;
	cpu->eff_addr = addr + val;
	cpu->cycles += get_pagecross_penalty(cpu, addr);
}

static void update_eff_addr(CPU* cpu)
{
	switch (cpu->instr_amode)
	{
		/* Accumulator and implied addressing don't touch memory */
		case AMODE_ACC:
		case AMODE_IMP:
			cpu->eff_addr = 0;
			break;

		/* Immediate addressing: next byte is value */
		case AMODE_IMM:
			cpu->eff_addr = cpu->pc++;
			break;

		/* Zero page addressing: effective address is the next byte */
		case AMODE_ZPG:
			cpu->eff_addr = memory_get(cpu->nes, cpu->pc++);
			break;

		/* Zero page, X addressing: effective address is the next byte + X */
		case AMODE_ZPX:
			cpu->eff_addr = (memory_get(cpu->nes, cpu->pc++) + cpu->x) & 0xFF;
			break;

		/* Zero page, X addressing: effective address is the next byte + Y */
		case AMODE_ZPY:
			cpu->eff_addr = (memory_get(cpu->nes, cpu->pc++) + cpu->y) & 0xFF;
			break;

		/* Relative addressing: effective address is PC + the next byte */
		case AMODE_REL:
			cpu->eff_addr = (int8_t)memory_get(cpu->nes, cpu->pc);
			cpu->eff_addr += ++cpu->pc;
			break;

		/* Absolute addressing: the effective address is the next two bytes */
		case AMODE_ABS:
			cpu->eff_addr = memory_get16(cpu->nes, cpu->pc++);
			++cpu->pc;
			break;

		/* Absolute, X addressing: effective address is the next 2 bytes + X */
		case AMODE_ABX:
			set_eff_addr_abs_indexed(cpu, cpu->x);
			break;

		/* Absolute, Y addressing: effective address is the next 2 bytes + Y */
		case AMODE_ABY:
			set_eff_addr_abs_indexed(cpu, cpu->y);
			break;

		/* Indirect addressing: effective address is pointed to
		   by the next 2 bytes */
		case AMODE_IND:
			cpu->eff_addr = memory_get16_ind(cpu->nes, memory_get16(cpu->nes, cpu->pc++));
			++cpu->pc;
			break;

		/* Indexed indirect addressing: effecive address is pointed
		   to by X + the next byte */
		case AMODE_XID:
		{
			uint8_t ptr = (memory_get(cpu->nes, cpu->pc++) + cpu->x) & 0xFF;
			cpu->eff_addr = memory_get16_ind(cpu->nes, ptr);
			break;
		}

		/* Indirect indexed addressing: effective address is Y + address
		   pointed to by next byte */
		case AMODE_IDY:
		{
			uint16_t ptr = memory_get16_ind(cpu->nes, memory_get(cpu->nes, cpu->pc++) & 0xFF);
			cpu->eff_addr = ptr + cpu->y;
			cpu->cycles += get_pagecross_penalty(cpu, ptr);
			break;
		}
	}
}

/*#include <stdio.h>*/
uint16_t cpu_step(CPU* cpu)
{
	/*static FILE* log;*/

	uint16_t old_cycles = cpu->cycles;
	/*if (cpu->idle_cycles)
	{
		--cpu->idle_cycles;
		return 1;
	}

	if (cpu->oam_dma_started)
	{
		uint8_t val = memory_get(cpu->nes, cpu->oam_dma_addr++);
		ppu_oamdata_write(&cpu->nes->ppu, val);
		if (++cpu->oam_dma_bytes_copied == 0)
			cpu->oam_dma_started = 0;
		return 1;
	}*/

	/* Service pending interrupts */
	if (cpu->pending_interrupts & INT_RST)
	{
		cpu->pending_interrupts &= ~INT_RST;
		cpu_reset(cpu);
	}
	else if (cpu->pending_interrupts & INT_NMI)
	{
		cpu->pending_interrupts &= ~INT_NMI;
		jump_interrupt(cpu, ADDR_NMI);
		cpu->cycles += 7;
	}
	else if ((cpu->pending_interrupts & INT_IRQ) && !(cpu->p & FLAG_I))
	{
		cpu->pending_interrupts &= ~INT_IRQ;
		jump_interrupt(cpu, ADDR_IRQ);
		cpu->cycles += 7;
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

	cpu->cycles += instr_cycles[cpu->opcode];
	instructions[cpu->opcode](cpu);
	return (cpu->cycles - old_cycles);  /* Cycles taken */
}

void cpu_interrupt(CPU* cpu, Interrupt type)
{
	cpu->pending_interrupts |= type;
}

/*void cpu_begin_oam_dma(CPU* cpu, uint16_t addr_start)
{
	if (!cpu->oam_dma_started)
	{
		cpu->oam_dma_started = 1;
		cpu->oam_dma_addr = addr_start;
		cpu->idle_cycles += 1 + (cpu->cycles & 1);
		cpu->oam_dma_bytes_copied = 0;
	}
}*/

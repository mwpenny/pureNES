#include <stdlib.h>
#include "memory.h"

/* CPU memory map (from https://en.wikibooks.org/wiki/NES_Programming/Memory_Map)

$0000 to $01FF: RAM
  $0000 to $00FF: zero page
  $0100 to $01FF: stack
  $0200 to $07FF: general purpose RAM
  $0800 to $1FFF: mirrors $0000 to $07FF (repeats every 2048 bytes)
$2000 to $3FFF: PPU registers
  $2000: control register
  $2001: mask register
  $2002: status register
  $2003: OAM address
  $2004: OAM data
  $2005: scroll register
  $2006: read/write address
  $2007: data read/write
  $2008 to $3FFF: mirrors $2000 to $2007 (repeats every 8 bytes)
$4000 to $401F: APU and I/O registers
$4020 to $5FFF: expansion ROM
$6000 to $7FFF: SRAM
$8000 to $FFFF: PRG ROM banks
  $8000 to $BFFF: PRG ROM bank 1
  $C000 to $FFFF: PRG ROM bank 2  
    $FFFA/$FFFB: NMI handler
    $FFFC/$FFFD: reset handler
    $FFFE/$FFFF: IRQ/BRK handler

*/


uint8_t memory_get(Memory* mem, uint16_t addr)
{
	if (addr < 0x2000)
		return mem->ram[addr & 0x7FF];
	else if (addr < 0x4000)
		return ppu_read(mem->ppu, addr);
	/* TODO: read from other areas */
	else if (addr > 0x7FFF && addr < 0xC000)
		return mem->prg1[addr - 0x8000];
	else if (addr > 0xBFFF && addr < 0x10000)
		return mem->prg2[addr - 0xC000];
	else
		return 0;
}

uint16_t memory_get16(Memory* mem, uint16_t addr)
{
	return memory_get(mem, addr) | (memory_get(mem, addr+1) << 8);
}

uint16_t memory_get16_ind(Memory* mem, uint16_t addr)
{
	/* the 6502 doesn't handle page boundary crosses for indirect addressing properly
	   e.g. JMP ($80FF) will fetch the high byte from $8000, NOT $8100! */
	uint16_t hi_addr = (addr & 0xFF00) | (uint8_t)((addr & 0xFF)+1);
	return memory_get(mem, addr) | (memory_get(mem, hi_addr) << 8);
}

void memory_set(Memory* mem, uint16_t addr, uint8_t val)
{
	/* TODO: disallow writing into read-only memory */
	if (addr < 0x2000)
		mem->ram[addr & 0x7FF] = val;
	else if (addr < 0x4000)
		ppu_write(mem->ppu, addr, val);
	/* TODO: write to other areas */
	else if (addr > 0x7FFF && addr < 0xC000)
		mem->prg1[addr - 0x8000] = val;
	else if (addr > 0xBFFF && addr < 0x10000)
		mem->prg2[addr - 0xC000] = val;
}
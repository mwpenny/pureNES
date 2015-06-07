#include "memory.h"

/* NES memory map (from https://en.wikibooks.org/wiki/NES_Programming/Memory_Map)

0x0000 to 0x00FF: zero page
0x0100 to 0x01FF: stack
0x0200 to 0x07FF: general purpose RAM
0x0800 to 0x1FFF: mirrors 0x0000 to 0x07FF (repeats every 2048 bytes)
0x2000 to 0x2007: PPU registers
0x2008 to 0x3FFF: mirrors 0x2000 to 0x2007 (repeats every 8 bytes)
0x4000 to 0x401F: APU and I/O registers
0x4020 to 0x5FFF: expansion ROM
0x6000 to 0x7FFF: SRAM
0x8000 to 0xFFFF: PRG ROM
  mirrored at 0xC000 if only one bank
  0xFFFA/0xFFFB: NMI handler
  0xFFFC/0xFFFD: reset handler
  0xFFFE/0xFFFF: IRQ/BRK handler
*/
uint8_t* memory_map(uint16_t addr)
{
	if (addr < 0x2000)
		return &memory[addr % 2048];
	else if (addr < 0x4000);
	/* return ppu_registers[addr % 8]; */
	else if (addr < 0x4020);
	/* return apu_io_registers[addr]; */
	else if (addr < 0x6000);
	/* return expansion_rom[addr]; */
	else if (addr < 0x8000);
	/* return sram[addr]; */
	else if (addr < 0x10000);
	/* return rom[addr]; */
}

uint8_t memory_get(uint16_t addr)
{
	return *memory_map(addr);
}

uint16_t memory_get16(uint16_t addr)
{
	return *memory_map(addr) | (*memory_map(addr+1) << 8);
}

void memory_set(uint16_t addr, uint8_t val)
{
	/* TODO: disallow writing into read-only memory */
	*memory_map(addr) = val;
}
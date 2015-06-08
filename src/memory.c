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
uint8_t* memory_get_mapped(uint16_t addr)
{
	if (addr < 0x2000)
		return &ram[addr % 2048];
	else if (addr < 0x4000);
	/* return ppu_registers[addr % 8]; */
	else if (addr < 0x4020);
	/* return apu_io_registers[addr]; */
	else if (addr < 0x6000);
	/* return expansion_rom[addr]; */
	else if (addr < 0x8000);
	/* return sram[addr]; */
	else
		return &rom[(addr - 0x8000) % rom_size]; /* mirror if only one bank */
}

void memory_map_rom(uint8_t* rom_addr, uint32_t size)
{
	rom = rom_addr;
	rom_size = size;
}

uint8_t memory_get(uint16_t addr)
{
	if (addr >= 0x2000 && addr < 0x8000) return 0; /* TODO: remove after mapping fully implemented */
	return *memory_get_mapped(addr);
}

uint16_t memory_get16(uint16_t addr)
{
	if (addr >= 0x2000 && addr < 0x8000) return 0; /* TODO: remove after mapping fully implemented */
	return *memory_get_mapped(addr) | (*memory_get_mapped(addr+1) << 8);
}

uint16_t memory_get16_ind(uint16_t addr)
{
	if (addr >= 0x2000 && addr < 0x8000) return 0; /* TODO: remove after mapping fully implemented */

	/* the 6502 doesn't handle page boundary crosses for indirect addressing properly
	   e.g. JMP ($80FF) will fetch the high byte from $8000, NOT $8100! */
	uint16_t hi_addr = (addr & 0xFF00) | (uint8_t)((addr & 0xFF)+1);
	return *memory_get_mapped(addr) | (*memory_get_mapped(hi_addr) << 8);
}

void memory_set(uint16_t addr, uint8_t val)
{
	/* TODO: disallow writing into read-only memory */
	if (addr >= 0x2000 && addr < 0x8000) return; /* TODO: remove after mapping fully implemented */
	*memory_get_mapped(addr) = val;
}
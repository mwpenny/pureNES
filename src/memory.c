#include "memory.h"

/* TODO: is this function really necessary? */
void memory_map(uint8_t** ptr, uint8_t* dest)
{
	*ptr = dest;
}

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
uint8_t* memory_get_mapped_address(Memory* mem, uint16_t addr)
{
	if (addr < 0x2000)
		return &mem->ram[addr % 0x800];
	else if (addr < 0x4000)
		return &mem->ppu_reg[(addr % 0x2008) - 0x2000]; /* TODO: what about struct padding!? */
	else if (addr < 0x4020);
		/* return apu_io_registers[addr]; */
	else if (addr < 0x6000);
		/* return expansion_rom[addr]; */
	else if (addr < 0x8000);
		/* return sram[addr]; */
	else if (addr < 0xC000)
		return &mem->prg1[addr - 0x8000];
	else
		return &mem->prg2[addr - 0xC000];
}

uint8_t memory_get(Memory* mem, uint16_t addr)
{
	if (addr >= 0x4000 && addr < 0x8000) return 0; /* TODO: remove after mapping fully implemented */
	return *memory_get_mapped_address(mem, addr);
}

uint16_t memory_get16(Memory* mem, uint16_t addr)
{
	if (addr >= 0x4000 && addr < 0x8000) return 0; /* TODO: remove after mapping fully implemented */
	return *memory_get_mapped_address(mem, addr) | (*memory_get_mapped_address(mem, addr+1) << 8);
}

uint16_t memory_get16_ind(Memory* mem, uint16_t addr)
{
	if (addr >= 0x4000 && addr < 0x8000) return 0; /* TODO: remove after mapping fully implemented */

	/* the 6502 doesn't handle page boundary crosses for indirect addressing properly
	   e.g. JMP ($80FF) will fetch the high byte from $8000, NOT $8100! */
	{uint16_t hi_addr = (addr & 0xFF00) | (uint8_t)((addr & 0xFF)+1);
	return *memory_get_mapped_address(mem, addr) | 
		   (*memory_get_mapped_address(mem, hi_addr) << 8);}
}

void memory_set(Memory* mem, uint16_t addr, uint8_t val)
{
	/* TODO: disallow writing into read-only memory */
	if (addr >= 0x4000 && addr < 0x8000) return; /* TODO: remove after mapping fully implemented */
	*memory_get_mapped_address(mem, addr) = val;
}
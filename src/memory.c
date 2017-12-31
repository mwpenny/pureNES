#include <stdint.h>

#include "apu.h"
#include "controller.h"
#include "memory.h"
#include "ppu.h"

/* NES memory map
   $0000 to $01FF: RAM
     $0000 to $00FF: zero page
     $0100 to $01FF: stack
     $0200 to $07FF: general purpose RAM
     $0800 to $1FFF: mirrors $0000 to $07FF
   $2000 to $3FFF: PPU registers ($2008-$3FFF mirror $2000-$2007)
   $4000 to $401F: APU and I/O registers
   $4020 to $5FFF: expansion ROM
   $6000 to $7FFF: SRAM
   $8000 to $FFFF: PRG ROM banks
   $8000 to $BFFF: PRG ROM bank 1
   $C000 to $FFFF: PRG ROM bank 2
     $FFFA/$FFFB: NMI handler
     $FFFC/$FFFD: reset handler
     $FFFE/$FFFF: IRQ/BRK handler */

uint8_t memory_get(NES* nes, uint16_t addr)
{
	if (addr < 0x2000)
		return nes->ram[addr % RAMSIZE];
	else if (addr < 0x4000)
		return ppu_read(&nes->ppu, 0x2000 | (addr % 8));
	else if (addr == 0x4015)
		return apu_read(&nes->apu, addr);
	else if (addr == 0x4016)
		return controller_read_output(&nes->c1);
	/* TODO: finish this / support multiple controllers and/or peripherals */
	/*else if (addr == 0x4017)
		return controller_read_output(&nes->c2);*/
	/* TODO: read from other areas */
	else if (addr >= 0x8000 && addr < 0xFFFF)
		return cartridge_read(&nes->cartridge, addr);

	/* TODO: Don't silently fail like this. Should never happen though. */
	else
		return 0;
}

uint16_t memory_get16(NES* nes, uint16_t addr)
{
	/* 6502 is little endian */
	return memory_get(nes, addr) | (memory_get(nes, addr+1) << 8);
}

uint16_t memory_get16_ind(NES* nes, uint16_t addr)
{
	/* The 6502 doesn't handle page boundary crosses for indirect addressing
	   properly (e.g. JMP ($80FF) will fetch the high byte from $8000,
	   NOT $8100!) */
	uint16_t hi_addr = (addr & 0xFF00) | (uint8_t)((addr & 0xFF) + 1);
	return memory_get(nes, addr) | (memory_get(nes, hi_addr) << 8);
}

void memory_set(NES* nes, uint16_t addr, uint8_t val)
{
	/* TODO: disallow writing into read-only memory */
	if (addr < 0x2000)
		nes->ram[addr % RAMSIZE] = val;
	else if (addr < 0x4000 || addr == 0x4014)
		ppu_write(&nes->ppu, addr, val);
	else if (addr == 0x4015)
		apu_write(&nes->apu, addr, val);
	else if (addr == 0x4016)
		controllers_write_input(val);
	else if (addr < 0x4018)
		apu_write(&nes->apu, addr, val);
	/* TODO: write to other areas */
	else if (addr > 0x401F)
		cartridge_write(&nes->cartridge, addr, val);
	/*else if (addr > 0x7FFF && addr < 0xC000)
		nes->cartridge.prg_bank1[addr - 0x8000] = val;
	else if (addr > 0xBFFF && addr < 0x10000)
		nes->cartridge.prg_bank2[addr - 0xC000] = val;*/
}

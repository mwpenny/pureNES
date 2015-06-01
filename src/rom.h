/* rom.h */
/* iNES ROM header and parser definition */

#ifndef ROM_H
#define ROM_H

#include <stdio.h>
#include <string.h>

/* TODO: ERROR CHECKING */

/* iNES header. (defined at http://fms.komkon.org/EMUL8/NES.html#LABM

Byte     Contents
---------------------------------------------------------------------------
0-3      String "NES^Z" used to recognize .NES files.
4        Number of 16kB ROM banks.
5        Number of 8kB VROM banks.
6        bit 0     1 for vertical mirroring, 0 for horizontal mirroring.
         bit 1     1 for battery-backed RAM at $6000-$7FFF.
         bit 2     1 for a 512-byte trainer at $7000-$71FF.
         bit 3     1 for a four-screen VRAM layout. 
         bit 4-7   Four lower bits of ROM Mapper Type.
7        bit 0     1 for VS-System cartridges.
         bit 1-3   Reserved, must be zeroes!
         bit 4-7   Four higher bits of ROM Mapper Type.
8        Number of 8kB RAM banks. For compatibility with the previous
         versions of the .NES format, assume 1x8kB RAM page when this
         byte is zero.
9        bit 0     1 for PAL cartridges, otherwise assume NTSC.
         bit 1-7   Reserved, must be zeroes!
10-15    Reserved, must be zeroes!
16-...   ROM banks, in ascending order. If a trainer is present, its
         512 bytes precede the ROM bank contents.
...-EOF  VROM banks, in ascending order.
--------------------------------------------------------------------------- */

typedef struct
{
	char magic[4];				/* should be NES^Z */
	unsigned char rom_size;		/* number of 16kB ROM banks */
	unsigned char vrom_size;	/* number of 8kB VROM banks*/
	unsigned char flags1;		/* flag bitfield 1 */
	unsigned char flags2;		/* flag bitfield 2 */
	unsigned char ram_banks;	/* number of 8kB RAM banks */
	unsigned char flags3;		/* flag bitfield 3 (PAL/NTSC) */
	unsigned char reserved[6];	/* should be zeros */
} ROM_Header;

int rom_parse(FILE* rom, ROM_Header* header);


#endif
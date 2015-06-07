#ifndef MAPPERS_H
#define MAPPERS_H

/* Sources:
	FCEUX Source Code and http://wiki.nesdev.com/w/index.php/List_of_mappers
*/

char* mappers[] = {
	"NROM",
	"MMC1",
	"UxROM",
	"CNROM",
	"MMC3",
	"MMC5",
	"FFE Rev. A",
	"AxROM",
	"",
	"MMC2",
	"MMC4",
	"Color Dreams",
	"REX DBZ 5",
	"CPROM",
	"REX SL-1632 (MMC3)",
	"100-in-1 Contra Function 16",
	"NAMDAO 24C02",
	"FFE Rev. B",
	"Jaleco SS880006",
	"Namco 129/163",
	"",
	"Konami VRC2/VRC4 A",
	"Konami VRC2/VRC4 B",
	"Konami VRC2/VRC4 C",
	"Konami VRC6 Rev. A",
	"Konami VCR2/VRC4 Rev. D",
	"Konami VRC6 Rev. B",
	"CC-21 MI HUN CHE",
	"INL-ROM Multi-Discrete",
	"RET_CUFROM",
	"UNROM 512",
	"2A03 Puritans NSF Album",
	"Irem G-101",
	"Taito TC0190",
	"BNROM Nina-01",
	"SC-127 Wario Land 2",
	"TCX Policeman",
	"PAL-ZZ SMB/TETRIS/NWC",
	"Bit Corp.",
	"SUBOR_STUDYNGAME",
	"SMB2j FDS",
	"Caltron 6-in-1",
	"BIO MIRACLE FDS",
	"FDS SMB2j LF36",
	"MMC3 BMC PIRATE A",
	"MMC3 BMC PIRATE B",
	"RUMBLESTATION 15-in-1",
	"NES-QJ SSVB/NWC",
	"Taito TCxxx",
	"MMC3 BMC PIRATE C",
	"SMB2j FDS Rev. A",
	"11-in-1 Ball Series",
	"MMC3 BMC PIRATE D",
	"Supervision 16-in-1",
	"MBC_NOVELDIAMOND",
	"BTL_GENIUSMERIOBROS",
	"SMB3 Pirate",
	"SIMBPLE BMC PIRATE A",
	"SIMBPLE BMC PIRATE B",
	"T3H53",
	"SIMBPLE BMC PIRATE C",
	"20-in-1 KAISER Rev. A",
	"Super 700-in-1",
	"Hello Kitty 255 in 1",
	"Tengen RAMBO1",
	"Irem-H3001",
	"GNROM and compatible",
	"Sunsoft mapper #3",
	"Sunsoft Mapper #4",
	"SUNSOFT-5/FME-7",
	"BA KAMEN DISCRETE",
	"Camerica BF9093",
	"JALECO JF-17",
	"Konami VRC3",
	"TW MMC3+VRAM Rev. A",
	"Konami VRC1",
	"NAMCOT 108 Rev. A",
	"Irem LROG017",
	"Irem 74HC161/32",
	"AVE/C&E/TXC BOARD",
	"Taito X1-005 Rev. A",
	"",
	"Taito X1-017",
	"YOKO VRC Rev. B",
	"",
	"Konami VRC7",
	"Jaleco JF-13",
	"74*139/74 DISCRETE",
	"Namco 3433",
	"Sunsoft-3",
	"Hummer/JY Board",
	"EARLY HUMMER/JY BOARD",
	"Jaleco JF-19",
	"SUNSOFT-3R",
	"HVC-UN1ROM",
	"Namco 108 Rev. B",
	"Bandai OEKAKIDS",
	"Irem TAM-S1",
	"",
	"VS Uni/Dual- system",
	"",
	"",
	"",
	"BTL_2708 (FDS DokiDoki Full)",
	"Camerica Golden Five",
	"NES-EVENT NWC1990",
	"SMB3 PIRATE A",
	"MAGIC CORP A",
	"FDS UNROM BOARD",
	"",
	"",
	"",
	"Asfer/NTDEC Board",
	"Hacker/Sachen Board",
	"MMC3 SG PROT. A",
	"MMC3 PIRATE A",
	"MMC1/MMC3/VRC PIRATE",
	"Future Media Board",
	"TSKROM/TLSROM",
	"NES-TQROM",
	"FDS Tobidase Daisakusen",
	"MMC3 PIRATE PROT. A",
	"",
	"MMC3 PIRATE H2288",
	"",
	"FDS LH32",
	"Super Joy (MMC3)",
	"Double Dragon Pirate",
	"",
	"",
	"",
	"",
	"TXC/MGENIUS 22111",
	"SA72008",
	"MMC3 BMC PIRATE",
	"",
	"TCU02",
	"Sachen S8259D",
	"Sachen S8259B",
	"Sachen S8259C",
	"Jaleco JF-11/14",
	"Sachen S8259A",
	"UNLKS7032",
	"Sachen TCA01",
	"AGCI 50282",
	"Sachen SA72007",
	"Sachen SA0161M",
	"Sachen TCU01",
	"Sachen SA0037",
	"Sachen SA0036",
	"Sachen S74LS374N",
	"VS System VRC1",
	"Arkanoid 2, Bandai",
	"Bandai BA-JUMP2",
	"Namco 3453",
	"MMC1A",
	"Open Corp. Daou 306",
	"Bandai Barcode/Datach",
	"Tengen 800037 (Alien Syndrome)",
	"Bandai 24C01",
	"SA009",
	"",
	"UNL_FS304",
	"Nanjing Standard",
	"Waixing Final Fantasy V",
	"Fire Emblem (Unl)",
	"SUBOR Rev. A",
	"SUBOR Rev. B",
	"RacerMate Challenge II",
	"",
	"Fujiya Standard",
	"Kaiser KS7058",
	"TXC 22211B",
	"TXC 22211C",
	"",
	"Kaiser KS7022",
	"BMC_FK23C",
	"Henggedianzi Standard",
	"Waixing SGZLZ",
	"Henggedianzi XJZB",
	"UNROM (Crazy Climber)",
	"",
	"Super Donkey Kong",
	"Suikan Pipe",
	"SUNSOFT-1",
	"CNROM with CHR disable",
	"Fukutake SBX",
	"KOF 96",
	"Bandai Karaoke Studio",
	"Thunder Warrior",
	"",
	"Pirate TQROM variant",
	"TW MMC3+VRAM Rev. B",
	"NTDEC TC-112",
	"TW MMC3+VRAM Rev. C",
	"TW MMC3+VRAM Rev. D",
	"BBTL_SuperBros11",
	"UNL_SuperFighter3",
	"TW MMC3+VRAM Rev. E",
	"Waixing Type G",
	"1200-in-1, 36-in-1",
	"8-in-1, 21-in-1",
	"150-in-1",
	"35-in-1",
	"64-in-1",
	"15-in-1, 3-in-1 (MMC3)",
	"Namco 108 Rev. C (DxROM)",
	"TAITO X1-005 Rev. B",
	"Gouder 37017",
	"JY Company Type B",
	"Namco 175/340",
	"JY Company Type C",
	"Super HIK 300-in-1",
	"9999999-in-1",
	"Supergun 20-in-1",
	"Super Game",
	"RCM GS2015",
	"Golden Card 6-in-1",
	"MagicFloor",
	"UNL_A9746",
	"",
	"UNL_N625092",
	"BTL_DragonNinja",
	"Waixing Type I",
	"Waixing Type J",
	"71-in-1",
	"76-in-1",
	"1200-in-1",
	"Action 52",
	"31-in-1",
	"BMC Contra+22-in-1",
	"20-in-1",
	"BMC QUATTRO",
	"BMC 22+20-in-1 RST",
	"BMC MAXI",
	"Golden Game 150-in-1",
	"Game 800-in-1",
	"",
	"UNL_6035052",
	"",
	"C&E",
	"TXC MMXMDH Two",
	"Waixing 74HC161",
	"Sachen 74LS374N",
	"C&E DECATHLON",
	"Waixing MMC Type H",
	"C&E Fong Shen Bang",
	"",
	"",
	"Waixing w/Security",
	"Time Diver Avenger (MMC3)",
	"",
	"San Guo Zhi Pirate",
	"Dragon Ball Pirate",
	"BTL_PikachuY2K"
};

#endif
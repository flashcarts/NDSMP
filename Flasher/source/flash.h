/*---------------------------------------------------------
 GBAMP Firmware flasher
 Written by DarkFader
 
 flash.h
---------------------------------------------------------*/

/*
 * Includes
 */

#ifdef PRINT_CART_ADDR
	#include <stdio.h>
#endif

/*
 * Defines
 */

#define FLASHID_MANUFACTURER	0x0000FFFF
#define FLASHID_SST				0x000000BF
#define FLASHID_SST_512KB		0x278000BF		// SST39xF400A	4 Mib	512 KB
#define FLASHID_SST_1024KB		0x278100BF		// SST39xF800A	8 Mib	1024 KB

/*
 * Imports
 */

void Delay100ms();

#ifdef FLASH_LINKER
	void SetCartAddr(unsigned int addr);
	unsigned short PPReadWord();
	void PPWriteWord(unsigned short data);
#else
	#define SetCartAddr(addr)		unsigned short *p = (unsigned short *)(0x08000000 | (addr) << 1)
	#define PPReadWord()			(*p)
	#define PPWriteWord(data)		*p = data
#endif

/*
 * Prototypes
 */

void FlashUnlock();
unsigned int FlashToCart(unsigned int flash_addr);
unsigned int FlashID();
int FlashEraseChip();
int FlashEraseBootBlock();
void RomProgram(unsigned short *data, unsigned int addr, unsigned int length);
int RomVerify(unsigned short *data, unsigned int addr, unsigned int length);
void FlashProgram(unsigned short *data, unsigned int addr, unsigned int length);
int FlashVerify(unsigned short *data, unsigned int addr, unsigned int length);

/*
 * Functions
 */

// read cartridge bus
inline unsigned short CartRead(unsigned int rom_addr)
{
	SetCartAddr(rom_addr);
	return PPReadWord();
}

// write cartridge space
inline void CartWrite(unsigned int rom_addr, unsigned short data)
{
	SetCartAddr(rom_addr);
	PPWriteWord(data);
}

// read without mirrors
inline unsigned short RomRead(unsigned int rom_addr)
{
	SetCartAddr(rom_addr >> 13 << 16 | (rom_addr & 0x1FFF));
	return PPReadWord();
}

// write without mirrors
inline void RomWrite(unsigned int rom_addr, unsigned short data)
{
	SetCartAddr(rom_addr >> 13 << 16 | (rom_addr & 0x1FFF));
	PPWriteWord(data);
}

// read in flash order (slow!)
inline unsigned short FlashRead(unsigned int flash_addr)
{
	unsigned int cart_addr = FlashToCart(flash_addr);
	#ifdef PRINT_CART_ADDR
		printf("FlashRead(0x%X) -> CartRead(0x%X)\n", flash_addr, cart_addr);
	#endif
	SetCartAddr(cart_addr);
	return PPReadWord();
}

// write in flash order (slow!)
inline void FlashWrite(unsigned int flash_addr, unsigned short data)
{
	unsigned int cart_addr = FlashToCart(flash_addr);
	#ifdef PRINT_CART_ADDR
		printf("FlashWrite(0x%X, 0x%X) -> CartWrite(0x%X, 0x%X)\n", flash_addr, data, cart_addr, data);
	#endif
	SetCartAddr(cart_addr);
	PPWriteWord(data);
}

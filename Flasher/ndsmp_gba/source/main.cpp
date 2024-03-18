#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "gba_base.h"
#include "gba_video.h"
#include "gba_systemcalls.h"
#include "gba_interrupt.h"
#include "gba_input.h"

#include "r6502_portfont_bin.h"

void FlashApp();

void Delay100ms()
{
	for (int i=0; i<10; i++) VBlankIntrWait();
}

const u16 palette[] =
{
	RGB8(0,0,0),	//0x40,0x80,0xc0),
	RGB8(0xFF,0xFF,0xFF),
	RGB8(0xF5,0xFF,0xFF),
	RGB8(0xDF,0xFF,0xF2),
	RGB8(0xCA,0xFF,0xE2),
	RGB8(0xB7,0xFD,0xD8),
	RGB8(0x2C,0x4F,0x8B)
};



typedef u16 Map[32][32];
Map &map = *(Map *)MAP_BASE_ADR(31);



int x,y;

void gotoxy(int x, int y)
{
	::x = x;
	::y = y;
}

int xprintf(char *fmt...)
{
	va_list args;
	va_start(args, fmt);
	char buf[20*32+1];
	int n = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	
	for (char *p=buf; *p; p++)
	{
		if (*p == '\n' || x >= 30) { x = 0; y++; }
		if (*p >= ' ') map[y][x++] = *p - ' ';
	}

    return n;
}



int main()
{
	// Set up the interrupt handlers
	InitInterrupt();
	// Enable Vblank Interrupt to allow VBlankIntrWait
	EnableInterrupt(IE_VBL);

	// Allow Interrupts
	REG_IME = 1;

	// load the palette for the background, 7 colors
	vu16 *temppointer = BG_COLORS;
	for (int i=0; i<7; i++)
	{
		*temppointer++ = palette[i];
	}

	// load the font into gba video mem (48 characters, 4bit tiles)
	temppointer = (vu16 *)VRAM;
	for (int i=0; i<(48*32); i++)
	{
		*temppointer++ = *((u16 *)r6502_portfont_bin + i);
	}

	// set the screen base to 31 (0x600F800) and char base to 0 (0x6000000)
	BGCTRL[0] = SCREEN_BASE(31);

	// screen mode & background to display
	SetMode((LCDC_BITS)(MODE_0 | BG0_ON));

	// clear screen map
	for (int i=0; i<32*32; i++) map[0][i] = 0;
	x=0; y=0;

	FlashApp();

	while (1) VBlankIntrWait();
	return 0;
}

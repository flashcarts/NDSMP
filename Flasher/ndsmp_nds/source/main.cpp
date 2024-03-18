#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "nds.h"
#include "nds/arm9/console.h"

void FlashApp();

void Delay100ms()
{
	for (int i=0; i<10; i++) swiWaitForVBlank();
}


typedef u16 Map[32][32];
Map &map = *(Map *)BG_MAP_RAM_SUB(31);



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
		if (*p == '\n' || x >= 32) { x = 0; y++; }
		if (*p >= ' ') map[y][x++] = *p | 15 << 12;
	}

    return n;
}

int main(void)
{
	irqInit();
	irqSet(IRQ_VBLANK, 0);

	videoSetMode(0);
	videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG);

	SUB_BG0_CR = BG_MAP_BASE(31);
	//BG0_CR = BG_MAP_BASE(31);
	
	BG_PALETTE_SUB[255] = RGB15(31,31,31);
	//BG_PALETTE[255] = RGB15(31,31,31);
	
	//consoleInit() is a lot more flexible but this gets you up and running quick
	consoleInitDefault((u16*)SCREEN_BASE_BLOCK_SUB(31), (u16*)CHAR_BASE_BLOCK_SUB(0), 16);
	//consoleInitDefault((u16*)SCREEN_BASE_BLOCK(31), (u16*)CHAR_BASE_BLOCK(0), 16);
	extern u16 default_font[];
	//consoleInit(default_font, (u16*)CHAR_BASE_BLOCK_SUB(0), 256, 0, (u16*)SCREEN_BASE_BLOCK_SUB(31), CONSOLE_USE_COLOR255, 16);

//	consolePrintf("Hello DS devers\n");

	// clear screen map
	for (int i=0; i<32*32; i++) map[0][i] = 0;
	x=0; y=0;

	WAIT_CR =~ 0x809C;	// ARM9 control and fast cycle

	FlashApp();

/*
	consolePrintf("\n\n\tHello DS devers\n");
	consolePrintf("\twww.drunkencoders.com");
	
	while(1)
	{
		//move the cursor
		consolePrintSet(0,10);
		consolePrintf("Touch x = %04X\n", IPC->touchX);
		consolePrintf("Touch y = %04X\n", IPC->touchY);		
	
	}*/
	
	while (1) swiWaitForVBlank();

	return 0;
}

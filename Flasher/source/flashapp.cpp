/*---------------------------------------------------------
 GBAMP Firmware flasher
 Written by DarkFader
 
 flashapp.cpp
---------------------------------------------------------*/


/*
 * NDS/GBA
 */
#ifdef ARM9		// assume NDS
	#include "nds.h"
	
	#define ScanKeys		scanKeys
	#define KeysInit		keysInit
	#define KeysHeld		keysHeld
	#define KeysDown		keysDown
	#define KeysUp			keysUp
	#define VBlankIntrWait	swiWaitForVBlank
#else
	#include "gba_base.h"
	#include "gba_input.h"
	#include "gba_interrupt.h"
	#include "gba_systemcalls.h"
#endif

/*
 * Includes
 */
#include "flash.h"
#include "loader_bin.h"
#include "repair_bin.h"

/*
 * Imports
 */
int xprintf(char *fmt...);
void gotoxy(int x, int y);

/*
 * FlashApp
 */
void FlashApp()
{
	xprintf("GBA Movie Player flasher v1.2\n");
	xprintf(" by Rafael Vuijk (DarkFader) \n");
	xprintf("NDSMP - GBAMP FW Hack v2.11  \n");
	xprintf(" NDS support for GBAMP       \n");
	xprintf(" by Michael Chisholm (Chishm)\n");
	xprintf("\n");

	while (1)
	{
		gotoxy(0,6);
		FlashUnlock();
		unsigned int id = FlashID();
		if ((id & FLASHID_MANUFACTURER) == FLASHID_SST)
		{
			xprintf("Flash detected!     (%08X)\n", id);
			xprintf("To flash, hold L+R and press  \n");
			xprintf("  SELECT for bootstrap loader.\n");
			xprintf("  START for repair utility.   \n");

			ScanKeys();
			unsigned int held = KeysHeld();
			unsigned int down = KeysDown();
			if (((held & (KEY_L | KEY_R)) == (KEY_L | KEY_R)) && ((down == KEY_START) || (down == KEY_SELECT)))
			{
				int verify = -2;

				while (verify < 0)
				{
					if (down == KEY_SELECT)
					{
						if ((u32)loader_bin_size != 0x4000)
						{
							xprintf("Bootstrap loader is wrong size.\nAborting... ");
							verify = 0;
						}
						else
						{
							xprintf("Erasing... ");
							FlashEraseBootBlock();
							xprintf("done.\n");
							
							if (verify == -2) xprintf("Programming %u bytes...", (u32)loader_bin_size);
							RomProgram((u16 *)loader_bin, 0, (u32)loader_bin_size);
							verify = RomVerify((u16 *)loader_bin, 0, (u32)loader_bin_size);
						}
					}
					else if (down == KEY_START)
					{
						xprintf("Erasing... ");
						FlashEraseChip();
						xprintf("done.\n");
				
						if (verify == -2) xprintf("Programming %u bytes...", (u32)repair_bin_size);
						RomProgram((u16 *)repair_bin, 0, (u32)repair_bin_size);
						verify = RomVerify((u16 *)repair_bin, 0, (u32)repair_bin_size);
					}
					if (verify < 0) for (int i=0; i<100; i++) VBlankIntrWait();		// wait before retry
				}

				xprintf("done.\n");
				xprintf("It is now safe to turn off\nyour computer.\n");

				break;
			}
		}
		else
		{
			xprintf("Flash NOT detected! (%08X)\n", id);
			xprintf("Please insert a Movie Player. \n");
			xprintf("                              \n");
			xprintf("                              \n");
		}
	}
}

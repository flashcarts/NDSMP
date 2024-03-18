/*-----------------------------------------------------------------
 NDS MP
 GBAMP NDS Firmware Hack Version 2.0
 An NDS aware firmware patch for the GBA Movie Player.
 By Michael Chisholm (Chishm)
 
 All resetMemory and startBinary functions are based 
 on the MultiNDS loader by Darkain.
 Original source available at:
 http://cvs.sourceforge.net/viewcvs.py/ndslib/ndslib/examples/loader/boot/main.cpp

 Filesystem code based on GBAMP_CF.c by Chishm (me).
 
License:
 Copyright (C) 2005  Michael "Chishm" Chisholm

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 If you use this code, please give due credit and email me about your
 project at chishm@hotmail.com
 
Helpful information:
 This code runs from the first Shared IWRAM bank:
 0x037F8000 to 0x037FC000
------------------------------------------------------------------*/

#include <nds/jtypes.h>
#include <nds/dma.h>
#include <nds/system.h>
#include <nds/interrupts.h>
#include <nds/timers.h>
#define ARM9
#include <nds/memory.h>
#include <NDS/ARM9/video.h>
#undef ARM9
#include <nds/ARM7/audio.h>

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Important things
#define TEMP_MEM 0x027FE000
#define NDS_HEAD 0x027FFE00
#define TEMP_ARM9_START_ADDRESS (*(vu32*)0x027FFFF4)
#define STORED_FILE_CLUSTER (*(vu32*)0x027FFFFC)
#define STORED_FILE_SECTOR (*(vu32*)0x027FFFF8)
const char* bootName = "_BOOT_MP.NDS";


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Firmware stuff

#define FW_READ        0x03

void readFirmware(uint32 address, uint32 size, uint8 * buffer) {
  uint32 index;

  // Read command
  while (REG_SPICNT & SPI_BUSY);
  REG_SPICNT = SPI_ENABLE | SPI_CONTINUOUS | SPI_DEVICE_NVRAM;
  REG_SPIDATA = FW_READ;
  while (REG_SPICNT & SPI_BUSY);

  // Set the address
  REG_SPIDATA =  (address>>16) & 0xFF;
  while (REG_SPICNT & SPI_BUSY);
  REG_SPIDATA =  (address>>8) & 0xFF;
  while (REG_SPICNT & SPI_BUSY);
  REG_SPIDATA =  (address) & 0xFF;
  while (REG_SPICNT & SPI_BUSY);

  for (index = 0; index < size; index++) {
    REG_SPIDATA = 0;
    while (REG_SPICNT & SPI_BUSY);
    buffer[index] = REG_SPIDATA & 0xFF;
  }
  REG_SPICNT = 0;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// CF card stuff
//#define _CF_USE_DMA

//----------------------------------------------------------------
// Data	types
#ifndef	NULL
 #define	NULL	0
#endif

//---------------------------------------------------------------
// FAT constants

#define FILE_LAST 0x00
#define FILE_FREE 0xE5

#define ATTRIB_ARCH	0x20
#define ATTRIB_DIR	0x10
#define ATTRIB_LFN	0x0F
#define ATTRIB_VOL	0x08
#define ATTRIB_HID	0x02
#define ATTRIB_SYS	0x04
#define ATTRIB_RO	0x01

#define FAT16_ROOT_DIR_CLUSTER 0x00

// File Constants
#ifndef EOF
#define EOF -1
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2
#endif

//---------------------------------------------------------------
// CF Addresses & Commands

#define GAME_PAK		0x08000000			// Game pack start address

#define CF_REG_STS		*(vu16*)(GAME_PAK + 0x018C0000)	// Status of the CF Card / Device control
#define CF_REG_CMD		*(vu16*)(GAME_PAK + 0x010E0000)	// Commands sent to control chip and status return
#define CF_REG_ERR		*(vu16*)(GAME_PAK + 0x01020000)	// Errors / Features

#define CF_REG_SEC		*(vu16*)(GAME_PAK + 0x01040000)	// Number of sector to transfer
#define CF_REG_LBA1		*(vu16*)(GAME_PAK + 0x01060000)	// 1st byte of sector address
#define CF_REG_LBA2		*(vu16*)(GAME_PAK + 0x01080000)	// 2nd byte of sector address
#define CF_REG_LBA3		*(vu16*)(GAME_PAK + 0x010A0000)	// 3rd byte of sector address
#define CF_REG_LBA4		*(vu16*)(GAME_PAK + 0x010C0000)	// last nibble of sector address | 0xE0

#define CF_DATA			(vu16*)(GAME_PAK + 0x01000000)		// Pointer to buffer of CF data transered from card

// Card status
#define CF_STS_INSERTED		0x50
#define CF_STS_REMOVED		0x00
#define CF_STS_READY		0x58

#define CF_STS_DRQ			0x08
#define CF_STS_BUSY			0x80

// Card commands
#define CF_CMD_LBA			0xE0
#define CF_CMD_READ			0x20
#define CF_CMD_WRITE		0x30

#define CARD_TIMEOUT	10000000

#define BYTE_PER_READ 512

//-----------------------------------------------------------------
// FAT constants
#define CLUSTER_EOF_16	0xFFFF
#define	CLUSTER_EOF	0x0FFFFFFF
#define CLUSTER_FREE	0x0000
#define CLUSTER_FIRST	0x0002

#define ATTRIB_ARCH	0x20
#define ATTRIB_DIR	0x10
#define ATTRIB_LFN	0x0F
#define ATTRIB_VOL	0x08
#define ATTRIB_HID	0x02
#define ATTRIB_SYS	0x04
#define ATTRIB_RO	0x01

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Data Structures

#define __PACKED __attribute__ ((__packed__))

// Boot Sector - must be packed
typedef struct
{
	__PACKED	u8	jmpBoot[3];
	__PACKED	u8	OEMName[8];
	// BIOS Parameter Block
	__PACKED	u16	bytesPerSector;
	__PACKED	u8	sectorsPerCluster;
	__PACKED	u16	reservedSectors;
	__PACKED	u8	numFATs;
	__PACKED	u16	rootEntries;
	__PACKED	u16	numSectorsSmall;
	__PACKED	u8	mediaDesc;
	__PACKED	u16	sectorsPerFAT;
	__PACKED	u16	sectorsPerTrk;
	__PACKED	u16	numHeads;
	__PACKED	u32	numHiddenSectors;
	__PACKED	u32	numSectors;
	union	// Different types of extended BIOS Parameter Block for FAT16 and FAT32
	{
		struct  
		{
			// Ext BIOS Parameter Block for FAT16
			__PACKED	u8	driveNumber;
			__PACKED	u8	reserved1;
			__PACKED	u8	extBootSig;
			__PACKED	u32	volumeID;
			__PACKED	u8	volumeLabel[11];
			__PACKED	u8	fileSysType[8];
			// Bootcode
			__PACKED	u8	bootCode[448];
		}	fat16;
		struct  
		{
			// FAT32 extended block
			__PACKED	u32	sectorsPerFAT32;
			__PACKED	u16	extFlags;
			__PACKED	u16	fsVer;
			__PACKED	u32	rootClus;
			__PACKED	u16	fsInfo;
			__PACKED	u16	bkBootSec;
			__PACKED	u8	reserved[12];
			// Ext BIOS Parameter Block for FAT16
			__PACKED	u8	driveNumber;
			__PACKED	u8	reserved1;
			__PACKED	u8	extBootSig;
			__PACKED	u32	volumeID;
			__PACKED	u8	volumeLabel[11];
			__PACKED	u8	fileSysType[8];
			// Bootcode
			__PACKED	u8	bootCode[420];
		}	fat32;
	}	extBlock;

	__PACKED	u16	bootSig;

}	BOOT_SEC;

// Directory entry - must be packed
typedef struct
{
	__PACKED	u8	name[8];
	__PACKED	u8	ext[3];
	__PACKED	u8	attrib;
	__PACKED	u8	reserved;
	__PACKED	u8	cTime_ms;
	__PACKED	u16	cTime;
	__PACKED	u16	cDate;
	__PACKED	u16	aDate;
	__PACKED	u16	startClusterHigh;
	__PACKED	u16	mTime;
	__PACKED	u16	mDate;
	__PACKED	u16	startCluster;
	__PACKED	u32	fileSize;
}	DIR_ENT;

#undef __PACKED

// File information - no need to pack
typedef struct
{
	u32 firstCluster;
	u32 length;
	u32 curPos;
	u32 curClus;			// Current cluster to read from
	int curSect;			// Current sector within cluster
	int curByte;			// Current byte within sector
	char readBuffer[512];	// Buffer used for unaligned reads
	u32 appClus;			// Cluster to append to
	int appSect;			// Sector within cluster for appending
	int appByte;			// Byte within sector for appending
	bool read;	// Can read from file
	bool write;	// Can write to file
	bool append;// Can append to file
	bool inUse;	// This file is open
	u32 dirEntSector;	// The sector where the directory entry is stored
	int dirEntOffset;	// The offset within the directory sector
}	FAT_FILE;


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Global Variables

// _VARS_IN_RAM variables are stored in the largest section of WRAM 
// available: IWRAM on NDS ARM7, EWRAM on NDS ARM9 and GBA

// Locations on card
int discRootDir;
int discRootDirClus;
int discFAT;
int discSecPerFAT;
int discNumSec;
int discData;
int discBytePerSec;
int discSecPerClus;
int discBytePerClus;

enum {FS_UNKNOWN, FS_FAT12, FS_FAT16, FS_FAT32} discFileSystem;

// Global sector buffer to save on stack space
unsigned char globalBuffer[BYTE_PER_READ];

/*-----------------------------------------------------------------
CF_IsInserted
Is a compact flash card inserted?
bool return OUT:  true if a CF card is inserted
-----------------------------------------------------------------*/
bool CF_IsInserted (void) 
{
	// Change register, then check if value did change
	CF_REG_STS = CF_STS_INSERTED;
	return (CF_REG_STS == CF_STS_INSERTED);
}


/*-----------------------------------------------------------------
CF_ReadSector
Read 512 byte sector numbered "sector" into "buffer"
u32 sector IN: address of 512 byte sector on CF card to read
void* buffer OUT: pointer to 512 byte buffer to store data in
bool return OUT: true if successful
-----------------------------------------------------------------*/
bool CF_ReadSector (u32 sector, void* buffer)
{
	int i;
	#ifndef _CF_USE_DMA
	 u16 *buff;
	#endif
	
	// Wait until CF card is finished previous commands
	i=0;
	while ((CF_REG_CMD & CF_STS_BUSY) && (i < CARD_TIMEOUT))
	{
		i++;
	}

	// Wait until card is ready for commands
	i = 0;
	while ((!(CF_REG_STS & CF_STS_INSERTED)) && (i < CARD_TIMEOUT))
	{
		i++;
	}
	if (i >= CARD_TIMEOUT)
		return false;

	// Only read one sector
	CF_REG_SEC = 0x01;	
	
	// Set read sector
	CF_REG_LBA1 = sector & 0xFF;						// 1st byte of sector number
	CF_REG_LBA2 = (sector >> 8) & 0xFF;					// 2nd byte of sector number
	CF_REG_LBA3 = (sector >> 16) & 0xFF;				// 3rd byte of sector number
	CF_REG_LBA4 = ((sector >> 24) & 0x0F )| CF_CMD_LBA;	// last nibble of sector number
	
	// Set command to read
	CF_REG_CMD = CF_CMD_READ;
	
	// Wait until card is ready for reading
	i = 0;
	while ((CF_REG_STS != CF_STS_READY) && (i < CARD_TIMEOUT))
	{
		i++;
	}
	if (i >= CARD_TIMEOUT)
		return false;
	
	// Read data
	#ifdef _CF_USE_DMA
	 DMA_SRC(3)  = (uint32)CF_DATA;
	 DMA_DEST(3) = (uint32)buffer;
	 DMA_CR(3)   = DMA_COPY_HALFWORDS | DMA_SRC_FIX | (BYTE_PER_READ>>1);
	 while(DMA_CR(3) & DMA_BUSY);
	#else
	 i=256;
   	 buff= (u16*)buffer;
  	 while(i--)
      	 	*buff++ = *CF_DATA; 
	#endif
	return true;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//FAT routines

#define FAT_ClustToSect(m) \
	(((m-2) * discSecPerClus) + discData)

/*-----------------------------------------------------------------
FAT_NextCluster
Internal function - gets the cluster linked from input cluster
-----------------------------------------------------------------*/
u32 FAT_NextCluster(u32 cluster)
{
	u32 nextCluster = CLUSTER_FREE;
	u32 sector;
	int offset;
	
	
	switch (discFileSystem) 
	{
		case FS_UNKNOWN:
			nextCluster = CLUSTER_FREE;
			break;
			
		case FS_FAT12:
			sector = discFAT + (((cluster * 3) / 2) / BYTE_PER_READ);
			offset = ((cluster * 3) / 2) % BYTE_PER_READ;
			CF_ReadSector(sector, globalBuffer);
			nextCluster = ((u8*) globalBuffer)[offset];
			offset++;
			
			if (offset >= BYTE_PER_READ) {
				offset = 0;
				sector++;
			}
			
			CF_ReadSector(sector, globalBuffer);
			nextCluster |= (((u8*) globalBuffer)[offset]) << 8;
			
			if (cluster & 0x01) {
				nextCluster = nextCluster >> 4;
			} else 	{
				nextCluster &= 0x0FFF;
			}
			
			break;
			
		case FS_FAT16:
			sector = discFAT + ((cluster << 1) / BYTE_PER_READ);
			offset = cluster % (BYTE_PER_READ >> 1);
			
			CF_ReadSector(sector, globalBuffer);
			// read the nextCluster value
			nextCluster = ((u16*)globalBuffer)[offset];
			
			if (nextCluster >= 0xFFF7)
			{
				nextCluster = CLUSTER_EOF;
			}
			break;
			
		case FS_FAT32:
			sector = discFAT + ((cluster << 2) / BYTE_PER_READ);
			offset = cluster % (BYTE_PER_READ >> 2);
			
			CF_ReadSector(sector, globalBuffer);
			// read the nextCluster value
			nextCluster = (((u32*)globalBuffer)[offset]) & 0x0FFFFFFF;
			
			if (nextCluster >= 0x0FFFFFF7)
			{
				nextCluster = CLUSTER_EOF;
			}
			break;
			
		default:
			nextCluster = CLUSTER_FREE;
			break;
	}
	
	return nextCluster;
}

/*-----------------------------------------------------------------
ucase
Returns the uppercase version of the given char
char IN: a character
char return OUT: uppercase version of character
-----------------------------------------------------------------*/
char ucase (char character)
{
	if ((character > 0x60) && (character < 0x7B))
		character = character - 0x20;
	return (character);
}

/*-----------------------------------------------------------------
FAT_InitFiles
Reads the FAT information from the CF card.
You need to call this before reading any files.
bool return OUT: true if successful.
-----------------------------------------------------------------*/
bool FAT_InitFiles (void)
{
	int i;
	int bootSector;
	BOOT_SEC* bootSec;
	
	// Arbitrary wait until the CF is ready (Approximately 1 second)
	for (i=0; (i < 1000000) && !CF_IsInserted(); i++) ;
	
	if (!CF_IsInserted())
	{
		return (false);
	}
	
	// Read first sector of CF card
	CF_ReadSector (0, globalBuffer);
	// Check if there is a FAT string, which indicates this is a boot sector
	if ((globalBuffer[0x36] == 'F') && (globalBuffer[0x37] == 'A') && (globalBuffer[0x38] == 'T'))
	{
		bootSector = 0;
	}
	// Check for FAT32
	else if ((globalBuffer[0x52] == 'F') && (globalBuffer[0x53] == 'A') && (globalBuffer[0x54] == 'T'))
	{
		bootSector = 0;
	}
	else	// This is an MBR
	{
		// Find first valid partition from MBR
		// First check for an active partition
		for (i=0x1BE; (i < 0x1FE) && (globalBuffer[i] != 0x80); i+= 0x10);
		// If it didn't find an active partition, search for any valid partition
		if (i == 0x1FE) 
			for (i=0x1BE; (i < 0x1FE) && (globalBuffer[i+0x04] == 0x00); i+= 0x10);
		
		// Go to first valid partition
		if ( i != 0x1FE)	// Make sure it found a partition
		{
			bootSector = globalBuffer[0x8 + i] + (globalBuffer[0x9 + i] << 8) + (globalBuffer[0xA + i] << 16) + ((globalBuffer[0xB + i] << 24) & 0x0F);
		} else {
			bootSector = 0;	// No partition found, assume this is a MBR free disk
		}
	}

	// Read in boot sector
	bootSec = (BOOT_SEC*) globalBuffer;
	CF_ReadSector (bootSector,  bootSec);
	
	// Store required information about the file system
	if (bootSec->sectorsPerFAT != 0)
	{
		discSecPerFAT = bootSec->sectorsPerFAT;
	}
	else
	{
		discSecPerFAT = bootSec->extBlock.fat32.sectorsPerFAT32;
	}
	
	if (bootSec->numSectorsSmall != 0)
	{
		discNumSec = bootSec->numSectorsSmall;
	}
	else
	{
		discNumSec = bootSec->numSectors;
	}

	discBytePerSec = BYTE_PER_READ;	// Sector size is redefined to be 512 bytes
	discSecPerClus = bootSec->sectorsPerCluster * bootSec->bytesPerSector / BYTE_PER_READ;
	discBytePerClus = discBytePerSec * discSecPerClus;
	discFAT = bootSector + bootSec->reservedSectors;

	discRootDir = discFAT + (bootSec->numFATs * discSecPerFAT);
	discData = discRootDir + ((bootSec->rootEntries * sizeof(DIR_ENT)) / BYTE_PER_READ);

	if ((discNumSec - discData) / bootSec->sectorsPerCluster < 4085)
	{
		discFileSystem = FS_FAT12;
	}
	else if ((discNumSec - discData) / bootSec->sectorsPerCluster < 65525)
	{
		discFileSystem = FS_FAT16;
	}
	else
	{
		discFileSystem = FS_FAT32;
	}

	if (discFileSystem != FS_FAT32)
	{
		discRootDirClus = FAT16_ROOT_DIR_CLUSTER;
	}
	else	// Set up for the FAT32 way
	{
		discRootDirClus = bootSec->extBlock.fat32.rootClus;
		// Check if FAT mirroring is enabled
		if (!(bootSec->extBlock.fat32.extFlags & 0x80))
		{
			// Use the active FAT
			discFAT = discFAT + ( discSecPerFAT * (bootSec->extBlock.fat32.extFlags & 0x0F));
		}
	}

	return (true);
}


/*-----------------------------------------------------------------
getBootFileCluster
-----------------------------------------------------------------*/
u32 getBootFileCluster (void)
{
	DIR_ENT dir;
	int firstSector = 0;
	bool notFound = false;
	bool found = false;
	int maxSectors;
	u32 wrkDirCluster = discRootDirClus;
	u32 wrkDirSector = 0;
	int wrkDirOffset = 0;
	int nameOffset;
	
	dir.startCluster = CLUSTER_FREE; // default to no file found
	dir.startClusterHigh = CLUSTER_FREE;
	

	// Check if fat has been initialised
	if (discBytePerSec == 0)
	{
		return (CLUSTER_FREE);
	}
	
	maxSectors = (wrkDirCluster == FAT16_ROOT_DIR_CLUSTER ? (discData - discRootDir) : discSecPerClus);
	// Scan Dir for correct entry
	firstSector = discRootDir;
	CF_ReadSector (firstSector + wrkDirSector, globalBuffer);
	found = false;
	notFound = false;
	wrkDirOffset = -1;	// Start at entry zero, Compensating for increment
	while (!found && !notFound) {
		wrkDirOffset++;
		if (wrkDirOffset == BYTE_PER_READ / sizeof (DIR_ENT))
		{
			wrkDirOffset = 0;
			wrkDirSector++;
			if ((wrkDirSector == discSecPerClus) && (wrkDirCluster != FAT16_ROOT_DIR_CLUSTER))
			{
				wrkDirSector = 0;
				wrkDirCluster = FAT_NextCluster(wrkDirCluster);
				if (wrkDirCluster == CLUSTER_EOF)
				{
					notFound = true;
				}
				firstSector = FAT_ClustToSect(wrkDirCluster);		
			}
			else if ((wrkDirCluster == FAT16_ROOT_DIR_CLUSTER) && (wrkDirSector == (discData - discRootDir)))
			{
				notFound = true;	// Got to end of root dir
			}
			CF_ReadSector (firstSector + wrkDirSector, globalBuffer);
		}
		dir = ((DIR_ENT*) globalBuffer)[wrkDirOffset];
		found = true;
		if ((dir.attrib & ATTRIB_DIR) || (dir.attrib & ATTRIB_VOL))
		{
			found = false;
		}
		for (nameOffset = 0; nameOffset < 8 && found; nameOffset++)
		{
			if (ucase(dir.name[nameOffset]) != bootName[nameOffset])
				found = false;
		}
		for (nameOffset = 0; nameOffset < 3 && found; nameOffset++)
		{
			if (ucase(dir.ext[nameOffset]) != bootName[nameOffset+9])
				found = false;
		}
		if (dir.name[0] == FILE_LAST)
		{
			notFound = true;
		}
	} 
	
	// If no file is found, return CLUSTER_FREE
	if (notFound)
	{
		return CLUSTER_FREE;
	}

	return (dir.startCluster | (dir.startClusterHigh << 16));
}

/*-----------------------------------------------------------------
fileRead(buffer, cluster, startOffset, length)
-----------------------------------------------------------------*/
u32 fileRead (char* buffer, u32 cluster, u32 startOffset, u32 length)
{
	int curByte;
	int curSect;
	
	int dataPos = 0;
	int chunks;
	int beginBytes;

	if (cluster == CLUSTER_FREE || cluster == CLUSTER_EOF) 
	{
		return 0;
	}
	
	// Follow cluster list until desired one is found
	for (chunks = startOffset / discBytePerClus; chunks > 0; chunks--)
	{
		cluster = FAT_NextCluster (cluster);
	}
	
	// Calculate the sector and byte of the current position,
	// and store them
	curSect = (startOffset % discBytePerClus) / BYTE_PER_READ;
	curByte = startOffset % BYTE_PER_READ;

	// Load sector buffer for new position in file
	CF_ReadSector( curSect + FAT_ClustToSect(cluster), globalBuffer);

	// Number of bytes needed to read to align with a sector
	beginBytes = (BYTE_PER_READ < length + curByte ? (BYTE_PER_READ - curByte) : length);

	// Read first part from buffer, to align with sector boundary
	for (dataPos = 0 ; dataPos < beginBytes; dataPos++)
	{
		buffer[dataPos] = globalBuffer[curByte++];
	}

	// Read in all the 512 byte chunks of the file directly, saving time
	for ( chunks = ((int)length - beginBytes) / BYTE_PER_READ; chunks > 0; chunks--)
	{
		curSect++;
		if (curSect >= discSecPerClus)
		{
			curSect = 0;
			cluster = FAT_NextCluster (cluster);
		}

		CF_ReadSector( curSect + FAT_ClustToSect( cluster), buffer + dataPos);
		dataPos += BYTE_PER_READ;
	}

	// Take care of any bytes left over before end of read
	if (dataPos < length)
	{

		// Update the read buffer
		curSect++;
		curByte = 0;
		if (curSect >= discSecPerClus)
		{
			curSect = 0;
			cluster = FAT_NextCluster (cluster);
		}
		CF_ReadSector( curSect + FAT_ClustToSect( cluster), globalBuffer);
		
		// Read in last partial chunk
		for (; dataPos < length; dataPos++)
		{
			buffer[dataPos] = globalBuffer[curByte];
			curByte++;
		}
	}
	
	return dataPos;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Loader functions

void CpuFastSet (u32 src, u32 dest, u32 ctrl)
{
	__asm volatile ("swi 0x0C0000\n");
}

static inline void dmaFillWords(const void* src, void* dest, uint32 size) {
	DMA_SRC(3)  = (uint32)src;
	DMA_DEST(3) = (uint32)dest;
	DMA_CR(3)   = DMA_COPY_WORDS | DMA_SRC_FIX | (size>>2);
	while(DMA_CR(3) & DMA_BUSY);
}

/*-------------------------------------------------------------------------
resetMemory1_ARM9
Clears the ARM9's icahce and dcache
Written by Darkain.
Modified by Chishm:
 Changed ldr to mov & add
 Added clobber list
--------------------------------------------------------------------------*/
#define resetMemory1_ARM9_size 0x400
void __attribute__ ((long_call)) resetMemory1_ARM9 (void) 
{
	REG_IME = 0;
	REG_IE = 0;
	REG_IF = ~0;

  __asm volatile(
    //clean and flush cache
    "mov r1, #0                   \n"
    "outer_loop:                  \n"
    " mov r0, #0                  \n"
    " inner_loop:                 \n"
    "  orr r2, r1, r0             \n"
    "  mcr p15, 0, r2, c7, c14, 2 \n"
    "  add r0, r0, #0x20          \n"
    "  cmp r0, #0x400             \n"
    " bne inner_loop              \n"
    " add r1, r1, #0x40000000     \n"
    " cmp r1, #0x0                \n"
    "bne outer_loop               \n"
  
    "mov r10, #0                  \n"
    "mcr p15, 0, r10, c7, c5, 0   \n"  //Flush ICache
    "mcr p15, 0, r10, c7, c6, 0   \n"  //Flush DCache
    "mcr p15, 0, r10, c7, c10, 4  \n"  //empty write buffer

	"mcr p15, 0, r10, c3, c0, 0   \n"  //disable write buffer       (def = 0)

    "mcr p15, 0, r10, c2, c0, 0   \n"  //disable DTCM and protection unit

    "mcr p15, 0, r10, c6, c0, 0   \n"  //disable protection unit 0  (def = 0)
    "mcr p15, 0, r10, c6, c1, 0   \n"  //disable protection unit 1  (def = 0)
    "mcr p15, 0, r10, c6, c2, 0   \n"  //disable protection unit 2  (def = 0)
    "mcr p15, 0, r10, c6, c3, 0   \n"  //disable protection unit 3  (def = 0)
    "mcr p15, 0, r10, c6, c4, 0   \n"  //disable protection unit 4  (def = ?)
    "mcr p15, 0, r10, c6, c5, 0   \n"  //disable protection unit 5  (def = ?)
    "mcr p15, 0, r10, c6, c6, 0   \n"  //disable protection unit 6  (def = ?)
    "mcr p15, 0, r10, c6, c7, 0   \n"  //disable protection unit 7  (def = ?)

    "mcr p15, 0, r10, c5, c0, 3   \n"  //IAccess
    "mcr p15, 0, r10, c5, c0, 2   \n"  //DAccess

	//"ldr r10, =0x0080000A         \n"	// Relocated code can't load data
	"mov r10, #0x00800000		  \n"	// Use mov instead
	"add r10, r10, #0x00A		  \n"
	"mcr p15, 0, r10, c9, c1, 0   \n"  //DTCM base  (def = 0x0080000A) ???
	
	//"ldr r10, =0x0000000C         \n"	// Relocated code can't load data
	"mov r10, #0x0000000C		  \n"	// Use mov instead
	"mcr p15, 0, r10, c9, c1, 1   \n"  //ITCM base  (def = 0x0000000C) ???

    "mov r10, #0x1F               \n"
	"msr cpsr, r10                \n"
	: // no outputs
	: // no inputs
	: "r0","r1","r2","r10"			// Clobbered registers
	);

	register int i;
	
	for (i=0; i<16*1024; i+=4) {  //first 16KB
		(*(vu32*)(i+0x00000000)) = 0x00000000;      //clear ITCM
		(*(vu32*)(i+0x00800000)) = 0x00000000;      //clear DTCM
	}
	
	for (i=16*1024; i<32*1024; i+=4) {  //second 16KB
		(*(vu32*)(i+0x00000000)) = 0x00000000;      //clear ITCM
	}

	(*(vu32*)0x00803FFC) = 0;   //IRQ_HANDLER ARM9 version
	(*(vu32*)0x00803FF8) = ~0;  //VBLANK_INTR_WAIT_FLAGS ARM9 version

	// Return to loop
	*((vu32*)0x027FFE04) = (u32)0xE59FF018;		// ldr pc, 0x027FFE24
	*((vu32*)0x027FFE24) = (u32)0x027FFE04;		// Set ARM9 Loop address
	__asm volatile("bx %0" : : "r" (0x027FFE04) ); 
}

/*-------------------------------------------------------------------------
resetMemory2_ARM9
Clears the ARM9's DMA channels and resets video memory
Written by Darkain.
Modified by Chishm:
 * Changed MultiNDS specific stuff
--------------------------------------------------------------------------*/
#define resetMemory2_ARM9_size 0x400
void __attribute__ ((long_call)) resetMemory2_ARM9 (void) 
{
 	register int i;
  
	//clear out ARM9 DMA channels
	for (i=0; i<4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
	}

	VRAM_CR = 0x80808080;
	(*(vu32*)0x027FFE04) = 0;   // temporary variable
	PALETTE[0] = 0xFFFF;
	dmaFillWords((void*)0x027FFE04, PALETTE+1, (2*1024)-2);
	dmaFillWords((void*)0x027FFE04, OAM,     2*1024);
	dmaFillWords((void*)0x027FFE04, (void*)0x04000000, 0x56);  //clear main display registers
	dmaFillWords((void*)0x027FFE04, (void*)0x04001000, 0x56);  //clear sub  display registers
	dmaFillWords((void*)0x027FFE04, VRAM,  656*1024);
	
	DISP_SR = 0;
	videoSetMode(0);
	videoSetModeSub(0);
	VRAM_A_CR = 0;
	VRAM_B_CR = 0;
	VRAM_C_CR = 0;
	VRAM_D_CR = 0;
	VRAM_E_CR = 0;
	VRAM_F_CR = 0;
	VRAM_G_CR = 0;
	VRAM_H_CR = 0;
	VRAM_I_CR = 0;
	VRAM_CR   = 0x03000000;
	POWER_CR  = 0x820F;

	//set shared ram to ARM7
	WRAM_CR = 0x03;

	// Return to loop
	*((vu32*)0x027FFE04) = (u32)0xE59FF018;		// ldr pc, 0x027FFE24
	*((vu32*)0x027FFE24) = (u32)0x027FFE04;		// Set ARM9 Loop address
	__asm volatile("bx %0" : : "r" (0x027FFE04) ); 
}


/*-------------------------------------------------------------------------
startBinary_ARM9
Jumps to the ARM9 NDS binary in sync with the display and ARM7
Written by Darkain.
Modified by Chishm:
 * Removed MultiNDS specific stuff
--------------------------------------------------------------------------*/
#define startBinary_ARM9_size 0xA0
void __attribute__ ((long_call)) startBinary_ARM9 (void)
{
	WAIT_CR = 0xE880;
	// set ARM9 load address to 0 and wait for it to change again
	(*(vu32*)0x027FFE24) = 0;
	while(DISP_Y!=191);
	while(DISP_Y==191);
	while ( (*(vu32*)0x027FFE24) == 0 );
	// wait for vblank
	while(DISP_Y!=191);
	while(DISP_Y==191);
	__asm volatile("bx %0" : : "r" (*(vu32*)0x027FFE24));
}

/*-------------------------------------------------------------------------
resetMemory_ARM7
Clears all of the NDS's RAM that is visible to the ARM7
Written by Darkain.
Modified by Chishm:
 * Added STMIA clear mem loop
--------------------------------------------------------------------------*/
void __attribute__ ((long_call)) resetMemory_ARM7 (void)
{
	int i;
	u8 settings1, settings2;
	
	REG_IME = 0;

	for (i=0; i<16; i++) {
		SCHANNEL_CR(i) = 0;
		SCHANNEL_TIMER(i) = 0;
		SCHANNEL_SOURCE(i) = 0;
		SCHANNEL_LENGTH(i) = 0;
	}
	SOUND_CR = 0;

	//clear out ARM7 DMA channels and timers
	for (i=0; i<4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
	}

	//switch to user mode
  __asm volatile(
	"mov r6, #0x1F                \n"
	"msr cpsr, r6                 \n"
	:
	:
	: "r6"
	);

  __asm volatile (
	// clear exclusive IWRAM
	// 0380:0000 to 0380:FFFF, total 64KiB
	"mov r0, #0 				\n"	
	"mov r1, #0 				\n"
	"mov r2, #0 				\n"
	"mov r3, #0 				\n"
	"mov r4, #0 				\n"
	"mov r5, #0 				\n"
	"mov r6, #0 				\n"
	"mov r7, #0 				\n"
	"mov r8, #0x03800000		\n"	// Start address 
	"mov r9, #0x03800000		\n" // End address part 1
	"orr r9, r9, #0x10000		\n" // End address part 2
	"clear_EIWRAM_loop:			\n"
	"stmia r8!, {r0, r1, r2, r3, r4, r5, r6, r7} \n"
	"cmp r8, r9					\n"
	"blt clear_EIWRAM_loop		\n"

	// clear most of EWRAM - except after 0x023FF800, which has DS settings
	"mov r8, #0x02000000		\n"	// Start address part 1 
	"orr r8, r8, #0x8000		\n" // Start address part 2
	"mov r9, #0x02300000		\n" // End address part 1
	"orr r9, r9, #0xff000		\n" // End address part 2
	"orr r9, r9, #0x00800		\n" // End address part 3
	"clear_EXRAM_loop:			\n"
	"stmia r8!, {r0, r1, r2, r3, r4, r5, r6, r7} \n"
	"cmp r8, r9					\n"
	"blt clear_EXRAM_loop		\n"
	:
	:
	: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9"
	);
  
	REG_IE = 0;
	REG_IF = ~0;
	(*(vu32*)(0x04000000-4)) = 0;  //IRQ_HANDLER ARM7 version
	(*(vu32*)(0x04000000-8)) = ~0; //VBLANK_INTR_WAIT_FLAGS, ARM7 version
	POWER_CR = 1;  //turn off power to stuffs
	
	// Reload DS Firmware settings
	readFirmware((u32)0x03FE70, 0x1, &settings1);
	readFirmware((u32)0x03FF70, 0x1, &settings2);
	
	if (settings1 > settings2) {
		readFirmware((u32)0x03FE00, 0x70, (u8*)0x027FFC80);
	} else {
		readFirmware((u32)0x03FF00, 0x70, (u8*)0x027FFC80);
	}
}


void __attribute__ ((long_call)) loadBinary_ARM7 (u32 fileCluster)
{
	u32 ndsHeader[0x170>>2];

	// read NDS header
	fileRead ((char*)ndsHeader, fileCluster, 0, 0x170);
	// read ARM9 info from NDS header
	u32 ARM9_SRC = ndsHeader[0x020>>2];
	char* ARM9_DST = (char*)ndsHeader[0x028>>2];
	u32 ARM9_LEN = ndsHeader[0x02C>>2];
	// read ARM7 info from NDS header
	u32 ARM7_SRC = ndsHeader[0x030>>2];
	char* ARM7_DST = (char*)ndsHeader[0x038>>2];
	u32 ARM7_LEN = ndsHeader[0x03C>>2];
	
	// Load binaries into memory
	fileRead(ARM9_DST, fileCluster, ARM9_SRC, ARM9_LEN);
	// Make sure ARM7 binary won't overwrite this loader. If it does,
	// store it in a temporary location near the end of main EWRAM
	if ((ARM7_DST >= (char*)0x037F8000) && (ARM7_DST < (char*)0x03800000))
	{
		if (ARM7_LEN > 0x8000)
			fileRead(ARM7_DST + 0x8000, fileCluster, ARM7_SRC + 0x8000, ARM7_LEN - 0x8000);
		fileRead((char*)(TEMP_MEM - 0x8000), fileCluster, ARM7_SRC, (ARM7_LEN > 0x8000 ? 0x8000 : ARM7_LEN));
	} else {
		fileRead(ARM7_DST, fileCluster, ARM7_SRC, ARM7_LEN);
	}
	if (((ndsHeader[0x070>>2]) != 0) && ((ndsHeader[0x04c>>2]) != 0))
	{
		__asm volatile ("swi 0x00");
	}

	// first copy the header to its proper location, excluding
	// the ARM9 start address, so as not to start it
	TEMP_ARM9_START_ADDRESS = ndsHeader[0x024>>2];		// Store for later
	ndsHeader[0x024>>2] = 0;
	dmaCopyWords(3, (void*)ndsHeader, (void*)NDS_HEAD, 0x170);
}

/*-------------------------------------------------------------------------
startBinary_ARM7
Jumps to the ARM7 NDS binary in sync with the display and ARM9
Written by Darkain.
Modified by Chishm:
 * Removed MultiNDS specific stuff
--------------------------------------------------------------------------*/
#define startBinary_ARM7_size 0xB0
void __attribute__ ((long_call)) startBinary_ARM7 (void)
{
	//clear shared RAM
	(*(vu32*)0x037F8000) = 0;   // temporary variable
	DMA_SRC(3)  = (uint32)0x037F8000;
	DMA_DEST(3) = (uint32)0x037F8000;
	DMA_CR(3)   = DMA_COPY_WORDS | DMA_SRC_FIX | ((32*1024)>>2);
	while(DMA_CR(3) & DMA_BUSY);

	while(DISP_Y!=191);
	while(DISP_Y==191);
	
	// copy NDS ARM9 start address into the header, starting ARM9
	((u32*)NDS_HEAD)[0x24>>2] = TEMP_ARM9_START_ADDRESS;

	while(DISP_Y!=191);
	while(DISP_Y==191);
	// Start ARM7
	__asm volatile("bx %0" : : "r" (*(vu32*)0x027FFE34));
}

/*-------------------------------------------------------------------------
startBinaryIWRAM_ARM7
Jumps to the ARM7 NDS binary in sync with the display and ARM9
Written by Darkain.
Modified by Chishm:
 * This function is used when the ARM7 binary is supposed to 
   be loaded into shared IWRAM
 * Removed MultiNDS specific stuff
Modified by Loopy (2005-12-17):
 * Changed startBinaryIWRAM_ARM7_size to 0xE0
--------------------------------------------------------------------------*/
#define startBinaryIWRAM_ARM7_size 0xE0
void __attribute__ ((long_call)) startBinaryIWRAM_ARM7 (void)
{
	// Copy shared IWRAM part of ARM7 binary to proper location
	DMA_SRC(3)  = (uint32)(TEMP_MEM-0x8000);
	DMA_DEST(3) = (uint32)0x037F8000;
	DMA_CR(3)   = DMA_COPY_WORDS | ((32*1024)>>2);
	while(DMA_CR(3) & DMA_BUSY);
	
	//clear the RAM used by the temporary ARM7 binary
	*(vu32*)(TEMP_MEM-0x8000) = 0;   // temporary variable
	DMA_SRC(3)  = (uint32)(TEMP_MEM-0x8000);
	DMA_DEST(3) = (uint32)(TEMP_MEM-0x8000);
	DMA_CR(3)   = DMA_COPY_WORDS | ((32*1024)>>2);
	while(DMA_CR(3) & DMA_BUSY);

	while(DISP_Y!=191);
	while(DISP_Y==191);
	
	// copy NDS ARM9 start address into the header, starting ARM9
	((u32*)NDS_HEAD)[0x24>>2] = TEMP_ARM9_START_ADDRESS;

	while(DISP_Y!=191);
	while(DISP_Y==191);
	// Start ARM7
	__asm volatile("bx %0" : : "r" (*(vu32*)0x027FFE34));
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// GBA mode functions

void __attribute__ ((long_call)) swapScreens_ARM9 (void) {
	// GBA Screen now on bottom
	vu16* lcdCtrl = (vu16*)0x04000304;		// Power control register
	*lcdCtrl  &= ~(1<<15);
	
	// Return to loop
	*((vu32*)0x027FFE04) = (u32)0xE59FF018;		// ldr pc, 0x027FFE24
	*((vu32*)0x027FFE24) = (u32)0x027FFE04;		// Set ARM9 Loop address
	__asm volatile("bx %0" : : "r" (0x027FFE04) ); 
}

void gotoGbaMode (void) {
	// Turn off unused screen
	vu32* startCtrl = (vu32*)0x027FFCE4;
	vu16* SPI_Ctrl = (vu16*)0x040001C0;
	vu16* SPI_Data = (vu16*)0x040001C2;
	
	while (*SPI_Ctrl & 0x80);	// Wait for SPI to be non-busy
	*SPI_Ctrl = 0x8800;			// 1MHz, Continuous mode, enable
	*SPI_Data = 0x00;			// Controlling the Backlights
	while (*SPI_Ctrl & 0x80);	// Wait for SPI to be non-busy
	*SPI_Ctrl = 0x8000;			// 1MHz, single byte, enable
	
	if (((*startCtrl >> 3) & 0x01) == 0x01)		// Bottom screen enabled if true, top if false
	{
		*SPI_Data = 0x05;		// Sound Power | Bottom Screen Power
		// ARM9 has to swap screens
		// copy ARM9 function to RAM, and make the ARM9 jump to it
		dmaCopyWords(3, (void*)swapScreens_ARM9, (void*)TEMP_MEM, 0x100);
		(*(vu32*)0x027FFE24) = (u32)TEMP_MEM;
		while ((*(vu32*)0x027FFE24) == (u32)TEMP_MEM);	// Wait for ARM9 to complete its task
	} else {
		*SPI_Data = 0x09;		// Sound Power | TOp Screen Power
	}
	while (*SPI_Ctrl & 0x80);	// Wait for SPI to be non-busy
	
	// Restart in gba mode
	__asm (".arm\n"
	"mov	r2, #0x40\n"
	"swi	0x1F0000\n"
	);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function

int main (void) {
	// Init card
	if(!FAT_InitFiles())
	{
		gotoGbaMode();
	}
	u32 fileCluster = STORED_FILE_CLUSTER;
	if ((fileCluster < 2) || (fileCluster >= CLUSTER_EOF))
	{
		fileCluster = getBootFileCluster();
	}
	if (fileCluster == CLUSTER_FREE)
	{
		gotoGbaMode();
	}
	
	// ARM9 clears its memory part 1
	// copy ARM9 function to RAM, and make the ARM9 jump to it
	dmaCopyWords(3, (void*)resetMemory1_ARM9, (void*)TEMP_MEM, resetMemory1_ARM9_size);
	(*(vu32*)0x027FFE24) = (u32)TEMP_MEM;	// Make ARM9 jump to the function	
	// Wait until the ARM9 has completed its task
	while ((*(vu32*)0x027FFE24) == (u32)TEMP_MEM);

	// ARM9 clears its memory part 2
	// copy ARM9 function to RAM, and make the ARM9 jump to it
	dmaCopyWords(3, (void*)resetMemory2_ARM9, (void*)TEMP_MEM, resetMemory2_ARM9_size);
	(*(vu32*)0x027FFE24) = (u32)TEMP_MEM;	// Make ARM9 jump to the function
	// Wait until the ARM9 has completed its task
	while ((*(vu32*)0x027FFE24) == (u32)TEMP_MEM);

	// Get ARM7 to clear RAM
	resetMemory_ARM7();	
	
	// ARM9 enters a wait loop
	// copy ARM9 function to RAM, and make the ARM9 jump to it
	dmaCopyWords(3, (void*)startBinary_ARM9, (void*)TEMP_MEM, startBinary_ARM9_size);
	(*(vu32*)0x027FFE24) = (u32)TEMP_MEM;	// Make ARM9 jump to the function
	
	// Load the NDS file
	loadBinary_ARM7(fileCluster);
	
	// Store start cluster and sector of loaded file, in case the loaded NDS wants it
	STORED_FILE_CLUSTER = fileCluster;
	STORED_FILE_SECTOR = FAT_ClustToSect(fileCluster);

	// ARM7 moved out of SIWRAM to clear it, then start binaries
	// copy ARM7 function to location just after ARM9 start function,
	// and make the ARM7 jump to it
	if ((((u32*)NDS_HEAD)[0x38>>2] >= (u32)0x037F8000) && (((u32*)NDS_HEAD)[0x38>>2] < (u32)0x03800000))
	{
		dmaCopyWords(3, (void*)startBinaryIWRAM_ARM7, (void*)(TEMP_MEM + startBinary_ARM9_size), startBinaryIWRAM_ARM7_size);
	} else {
		dmaCopyWords(3, (void*)startBinary_ARM7, (void*)(TEMP_MEM + startBinary_ARM9_size), startBinary_ARM7_size);
	}
	__asm volatile("bx %0" : : "r" (TEMP_MEM + startBinary_ARM9_size));		// Jump to new location

	while(1);
}


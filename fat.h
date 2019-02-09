/******************************************************************************/
/*                                                                            */
/* Project : FAT12/16 File System                                             */
/* File    : fat.h                                                            */
/* Author  : Kyoungmoon Sun(msg2me@msn.com)                                   */
/* Company : Dankook Univ. Embedded System Lab.                               */
/* Notes   : FAT File System header                                           */
/* Date    : 2008/7/2                                                         */
/*                                                                            */
/******************************************************************************/

#ifndef _FAT_H_
#define _FAT_H_

#include "common.h"
#include "disk.h"
#include "clusterlist.h"

#define FAT12					0
#define FAT16					1
#define FAT32					2

#define MAX_SECTOR_SIZE			512
#define MAX_NAME_LENGTH			256
#define MAX_ENTRY_NAME_LENGTH	11

#define ATTR_READ_ONLY			0x01
#define ATTR_HIDDEN				0x02
#define ATTR_SYSTEM				0x04
#define ATTR_VOLUME_ID			0x08
#define ATTR_DIRECTORY			0x10
#define ATTR_ARCHIVE			0x20
#define ATTR_LONG_NAME			ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID

#define VOLUME_LABEL			"FAT BY SKM "
#define DIR_ENTRY_FREE			0xE5
#define DIR_ENTRY_NO_MORE		0x00
#define DIR_ENTRY_OVERWRITE		1

#define SHUT_BIT_MASK16			0x8000
#define ERR_BIT_MASK16			0x4000

#define SHUT_BIT_MASK32			0x08000000
#define ERR_BIT_MASK32			0x04000000

#define EOC12					0x0FF8
#define EOC16					0xFFF8
#define EOC32					0x0FFFFFF8
#define FREE_CLUSTER			0x00

#define MS_EOC12				0x0FFF
#define MS_EOC16				0xFFFF
#define MS_EOC32				0x0FFFFFFF

#define SET_FIRST_CLUSTER( a, b )	{ ( a ).firstClusterHI = ( b ) >> 16; ( a ).firstClusterLO = ( WORD )( ( b ) & 0xFFFF ); }
#define GET_FIRST_CLUSTER( a )		( ( ( ( DWORD )( a ).firstClusterHI ) << 16 ) | ( a ).firstClusterLO )
//#define IS_POINT_ROOT_ENTRY( a )	( ( a ).attribute & ATTR_VOLUME_ID )
#define IS_POINT_ROOT_ENTRY( a )	( ( ( a ).attribute & ATTR_VOLUME_ID ) || ( ( ( a ).attribute & ATTR_DIRECTORY ) && ( ( a ).firstClusterLO == 0 ) ) || ( a ).name[0] == 32 )

/* FAT structures are written based on MS Hardware White Paper */
#ifdef _WIN32
#pragma pack(push,fatstructures)
#endif
#pragma pack(1)

typedef struct
{
	BYTE	driveNumber;//driver type ..ex) floppy, others..
	BYTE	reserved1;
	BYTE	bootSignature;//always 0x29
	DWORD	volumeID;//volume serial num
	BYTE	volumeLabel[11];//volume or disk name..? --BYTE is Unsigned char
	BYTE	filesystemType[8];//just..text
} FAT_BOOTSECTOR;

typedef struct
{
	BYTE	jmpBoot[3];//command to jump to the boot code
	BYTE	OEMName[8];//Just company name..

	UINT16	bytesPerSector;//ex.. 512 1024 2048 4096 (1 sector)
	UINT8	sectorsPerCluster;//must have multiple of 2
	UINT16	reservedSectorCount;
	UINT8	numberOfFATs;//number of FAT region
	UINT16	rootEntryCount;//how many entries will RootDir accommodate
	UINT16	totalSectors;//Number of sectors on a volume not a storage

	BYTE	media;//usb orCFCard maybe?

	UINT16	FATSize16;//only use for FAT16 :: Number of sectors int FAT area
	UINT16	sectorsPerTrack;
	UINT16	numberOfHeads;//on storage
	UINT32	hiddenSectors;
	UINT32	totalSectors32;//FAT16 can write '0' , FAT32 use this

	union// bs,BPB32,padding이 메모리를 공유해서 사용. why do they use like this?
	{
		FAT_BOOTSECTOR bs;

		struct
		{
			UINT32	FATSize32;
			WORD	extFlags;
			WORD	FSVersion;
			UINT32	rootCluster;
			WORD	FSInfo;
			UINT16	backupBootSectors;
			BYTE	reserved[12];
			FAT_BOOTSECTOR bs;
		} BPB32;

		char padding[512 - 36];
	};
} FAT_BPB;//BIOS Parameter block

typedef struct
{
	DWORD	leadSignature;
	BYTE	reserved1[480];
	DWORD	structSignature;
	UINT32	freeCount;
	UINT32	nextFree;
	BYTE	reserved2[12];
	DWORD	trailSignature;
} FAT_FSINFO;

typedef struct
{
	BYTE	name[11];
	BYTE	attribute;
	BYTE	NTReserved;

	BYTE	createdTimeThen;
	WORD	createdTime;
	WORD	createdDate;

	WORD	lastAccessDate;

	WORD	firstClusterHI;

	WORD	writeTime;
	WORD	writeData;

	WORD	firstClusterLO;

	UINT32	fileSize;
} FAT_DIR_ENTRY;

#ifdef _WIN32
#pragma pack(pop, fatstructures)
#else
#pragma pack()
#endif

typedef struct
{
	BYTE			FATType;
	DWORD			FATSize;
	DWORD			EOCMark;
	FAT_BPB			bpb;
	CLUSTER_LIST	freeClusterList;
	DISK_OPERATIONS*	disk;

	union
	{
		FAT_FSINFO	info32;
		struct
		{
			UINT32	freeCount;
			UINT32	nextFree;
		} info;
	};
} FAT_FILESYSTEM;

typedef struct
{
	WORD	year;
} FAT_FILETIME;

typedef struct
{
	UINT32	cluster;
	UINT32	sector;
	INT32	number;		/* in the sector */
} FAT_ENTRY_LOCATION;

typedef struct
{
	FAT_FILESYSTEM*		fs;
	FAT_DIR_ENTRY		entry;
	FAT_ENTRY_LOCATION	location;
} FAT_NODE;

typedef int ( *FAT_NODE_ADD )( void*, FAT_NODE* );

void fat_umount( FAT_FILESYSTEM* fs );
int fat_read_superblock( FAT_FILESYSTEM* fs, FAT_NODE* root );
int fat_read_dir( FAT_NODE* dir, FAT_NODE_ADD adder, void* list );
int fat_mkdir( const FAT_NODE* parent, const char* entryName, FAT_NODE* retEntry );
int fat_rmdir( FAT_NODE* node );
int fat_lookup( FAT_NODE* parent, const char* entryName, FAT_NODE* retEntry );
int fat_create( FAT_NODE* parent, const char* entryName, FAT_NODE* retEntry );
int fat_read( FAT_NODE* file, unsigned long offset, unsigned long length, char* buffer );
int fat_write( FAT_NODE* file, unsigned long offset, unsigned long length, const char* buffer );
int fat_remove( FAT_NODE* file );
int fat_df( FAT_FILESYSTEM* fs, UINT32* totalSectors, UINT32* usedSectors );

#endif


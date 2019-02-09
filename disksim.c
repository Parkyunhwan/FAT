/******************************************************************************/
/*                                                                            */
/* Project : FAT12/16 File System                                             */
/* File    : disksim.c                                                        */
/* Author  : Kyoungmoon Sun(msg2me@msn.com)                                   */
/* Company : Dankook Univ. Embedded System Lab.                               */
/* Notes   : Disk simulator                                                   */
/* Date    : 2008/7/2                                                         */
/*                                                                            */
/******************************************************************************/

#include <stdlib.h>
#include <memory.h>
#include "fat.h"
#include "disk.h"
#include "disksim.h"

typedef struct
{
	char*	address;
} DISK_MEMORY;

int disksim_read( DISK_OPERATIONS* this, SECTOR sector, void* data );
int disksim_write( DISK_OPERATIONS* this, SECTOR sector, const void* data );

int disksim_init( SECTOR numberOfSectors, unsigned int bytesPerSector, DISK_OPERATIONS* disk ) // 초기화
{
	if( disk == NULL ) //디스크 공간X
		return -1;

	disk->pdata = malloc( sizeof( DISK_MEMORY ) ); 
	if( disk->pdata == NULL )
	{
		disksim_uninit( disk );
		return -1;
	}

	( ( DISK_MEMORY* )disk->pdata )->address = ( char* )malloc( bytesPerSector * numberOfSectors );
	if( disk->pdata == NULL )
	{
		disksim_uninit( disk );
		return -1;
	}

	disk->read_sector	= disksim_read;
	disk->write_sector	= disksim_write;
	disk->numberOfSectors	= numberOfSectors;
	disk->bytesPerSector	= bytesPerSector;

	return 0;
}

void disksim_uninit( DISK_OPERATIONS* this )
{
	if( this )
	{
		if( this->pdata )
			free( this->pdata );
	}
}

int disksim_read( DISK_OPERATIONS* this, SECTOR sector, void* data )
{
	char* disk = ( ( DISK_MEMORY* )this->pdata )->address; // 처리 할 섹터의 데이터 주소

	if( sector < 0 || sector >= this->numberOfSectors )
		return -1;

	memcpy( data, &disk[sector * this->bytesPerSector], this->bytesPerSector ); //디스크의 데이터를 data로 복사

	return 0;
}

int disksim_write( DISK_OPERATIONS* this, SECTOR sector, const void* data )
{
	char* disk = ( ( DISK_MEMORY* )this->pdata )->address; // 처리할 디스크 주소

	if( sector < 0 || sector >= this->numberOfSectors )
		return -1;

	memcpy( &disk[sector * this->bytesPerSector], data, this->bytesPerSector ); // data를 디스크에 쓰기

	return 0;
}


/******************************************************************************/
/*                                                                            */
/* Project : FAT12/16 File System                                             */
/* File    : fat.c                                                            */
/* Author  : Kyoungmoon Sun(msg2me@msn.com)                                   */
/* Company : Dankook Univ. Embedded System Lab.                               */
/* Notes   : FAT File System core                                             */
/* Date    : 2008/7/2                                                         */
/*                                                                            */
/******************************************************************************/

#include "fat.h"
#include "clusterlist.h"

#define MIN( a, b )					( ( a ) < ( b ) ? ( a ) : ( b ) )
#define MAX( a, b )					( ( a ) > ( b ) ? ( a ) : ( b ) )
#define NO_MORE_CLUSER()			WARNING( "No more clusters are remained\n" );

unsigned char toupper( unsigned char ch );
int isalpha( unsigned char ch );
int isdigit( unsigned char ch );

/* calculate the 'sectors per cluster' by some conditions */
DWORD get_sector_per_clusterN( DWORD diskTable[][2], UINT64 diskSize, UINT32 bytesPerSector )
{
	int i = 0;

	do
	{
		if( ( ( UINT64 )( diskTable[i][0] * 512 ) ) >= diskSize )
			return diskTable[i][1] / ( bytesPerSector / 512 );
	}
	while( diskTable[i++][0] < 0xFFFFFFFF );

	return 0;
}

DWORD get_sector_per_cluster16( UINT64 diskSize, UINT32 bytesPerSector )
{
	DWORD	diskTableFAT16[][2] =
	{
		{ 8400,			0	},
		{ 32680,		2	},
		{ 262144,		4	},
		{ 524288,		8	},
		{ 1048576,		16	},
		/* The entries after this point are not used unless FAT16 is forced */
		{ 2097152,		32	},
		{ 4194304,		64	},
		{ 0xFFFFFFFF,	0	}
	};

	return get_sector_per_clusterN( diskTableFAT16, diskSize, bytesPerSector );
}

DWORD get_sector_per_cluster32( UINT64 diskSize, UINT32 bytesPerSector )
{
	DWORD	diskTableFAT32[][2] =
	{
		{ 66600,		0	},
		{ 532480,		1	},
		{ 16777216,		8	},
		{ 33554432,		16	},
		{ 67108864,		32	},
		{ 0xFFFFFFFF,	64	}
	};

	return get_sector_per_clusterN( diskTableFAT32, diskSize, bytesPerSector );
}

DWORD get_sector_per_cluster( BYTE FATType, UINT64 diskSize, UINT32 bytesPerSector )
{
	switch( FATType )
	{
		case 0:		/* FAT12 */
			return 1;
		case 1:		/* FAT16 */
			return get_sector_per_cluster16( diskSize, bytesPerSector );
		case 2:		/* FAT32 */
			return get_sector_per_cluster32( diskSize, bytesPerSector );
	}

	return 0;
}

/* fills the field FATSize16 and FATSize32 of the FAT_BPB */
void fill_fat_size( FAT_BPB* bpb, BYTE FATType )
{
	UINT32	diskSize = ( bpb->totalSectors32 == 0 ? bpb->totalSectors : bpb->totalSectors32 );
	UINT32	rootDirSectors = ( ( bpb->rootEntryCount * 32 ) + (bpb->bytesPerSector - 1) ) / bpb->bytesPerSector;
	UINT32	tmpVal1 = diskSize - ( bpb->reservedSectorCount + rootDirSectors );
	UINT32	tmpVal2 = ( 256 * bpb->sectorsPerCluster ) + bpb->numberOfFATs;
	UINT32	FATSize;

	if( FATType == FAT32 )
		tmpVal2 = tmpVal2 / 2;

	FATSize = ( tmpVal1 + ( tmpVal2 - 1 ) ) / tmpVal2;

	if( FATType == 32 )
	{
		bpb->FATSize16 = 0;
		bpb->BPB32.FATSize32 = FATSize;
	}
	else
		bpb->FATSize16 = ( WORD )( FATSize & 0xFFFF );
}

int fill_bpb( FAT_BPB* bpb, BYTE FATType, SECTOR numberOfSectors, UINT32 bytesPerSector )
{
	QWORD diskSize = numberOfSectors * bytesPerSector;
	FAT_BOOTSECTOR* bs;
	BYTE	filesystemType[][8] = { "FAT12   ", "FAT16   ", "FAT32   " };
	UINT32	sectorsPerCluster;

	if( FATType > 2 )
		return FAT_ERROR;

	ZeroMemory( bpb, sizeof( FAT_BPB ) );

	bpb->jmpBoot[0] = 0xEB;
	bpb->jmpBoot[1] = 0x00;		/* ?? */
	bpb->jmpBoot[2] = 0x90;	
	memcpy( bpb->OEMName, "MSWIN4.1", 8 );

	sectorsPerCluster			= get_sector_per_cluster( FATType, diskSize, bytesPerSector );
	if( sectorsPerCluster == 0 )
	{
		WARNING( "The number of sector is out of range\n" );
		return -1;
	}

	bpb->bytesPerSector			= bytesPerSector;
	bpb->sectorsPerCluster		= sectorsPerCluster;
	bpb->reservedSectorCount	= ( FATType == FAT32 ? 32 : 1 );
	bpb->numberOfFATs			= 1;
	bpb->rootEntryCount			= ( FATType == FAT32 ? 0 : 512 );
	bpb->totalSectors			= ( numberOfSectors < 0x10000 ? ( UINT16 ) numberOfSectors : 0 );

	bpb->media					= 0xF8;
	fill_fat_size( bpb, FATType );
	bpb->sectorsPerTrack		= 0;
	bpb->numberOfHeads			= 0;
	bpb->totalSectors32			= ( numberOfSectors >= 0x10000 ? numberOfSectors : 0 );

	if( FATType == FAT32 )
	{
		bpb->BPB32.extFlags		= 0x0081;	/* active FAT : 1, only one FAT is active */
		bpb->BPB32.FSVersion	= 0;
//		bpb->BPB32.rootCluster	= 2;
		bpb->BPB32.FSInfo		= 1;
		bpb->BPB32.backupBootSectors	= 6;
		bpb->BPB32.backupBootSectors	= 0;
		ZeroMemory( bpb->BPB32.reserved, 12 );
	}

	if( FATType == FAT32 )
		bs = &bpb->BPB32.bs;
	else
		bs = &bpb->bs;

	if( FATType == FAT12 )
		bs->driveNumber	= 0x00;
	else
		bs->driveNumber	= 0x80;

	bs->reserved1		= 0;
	bs->bootSignature	= 0x29;
	bs->volumeID		= 0;
	memcpy( bs->volumeLabel, VOLUME_LABEL, 11 );
	memcpy( bs->filesystemType, filesystemType[FATType], 8 );

	return FAT_SUCCESS;
}

int get_fat_type( FAT_BPB* bpb )
{
	UINT32	totalSectors, dataSector, rootSector, countOfClusters, FATSize;

	rootSector = ( ( bpb->rootEntryCount * 32 ) + ( bpb->bytesPerSector - 1 ) ) / bpb->bytesPerSector;

	if( bpb->FATSize16 != 0 )
		FATSize = bpb->FATSize16;
	else
		FATSize = bpb->BPB32.FATSize32;

	if( bpb->totalSectors != 0 )
		totalSectors = bpb->totalSectors;
	else
		totalSectors = bpb->totalSectors32;

	dataSector = totalSectors - ( bpb->reservedSectorCount + ( bpb->numberOfFATs * FATSize ) + rootSector );
	countOfClusters = dataSector / bpb->sectorsPerCluster;

	if( countOfClusters < 4085 )
		return FAT12;
	else if( countOfClusters < 65525 )
		return FAT16;
	else
		return FAT32;

	return FAT_ERROR;
}

FAT_ENTRY_LOCATION get_entry_location( const FAT_DIR_ENTRY* entry )
{
	FAT_ENTRY_LOCATION	location;

	location.cluster	= GET_FIRST_CLUSTER( *entry );
	location.sector		= 0;
	location.number		= 0;

	return location;
}

/* fills the reserved fields of FAT */
int fill_reserved_fat( FAT_BPB* bpb, BYTE* sector )
{
	BYTE	FATType;
	DWORD*	shutErrBit12;
	WORD*	shutBit16;
	WORD*	errBit16;
	DWORD*	shutBit32;
	DWORD*	errBit32;

	FATType = get_fat_type( bpb );
	if( FATType == FAT12 )
	{
		shutErrBit12 = ( DWORD* )sector;

		*shutErrBit12 = 0xFF0 << 20;
		*shutErrBit12 |= ( ( DWORD )bpb->media & 0x0F ) << 20;
		*shutErrBit12 |= MS_EOC12 << 8;
	}
	else if( FATType == FAT16 )
	{
		shutBit16 = ( WORD* )sector;
		errBit16 = ( WORD* )sector + sizeof( WORD );

		*shutBit16 = 0xFFF0 | bpb->media;
		*errBit16 = MS_EOC16;
	}
	else
	{
		shutBit32 = ( DWORD* )sector;
		errBit32 = ( DWORD* )sector + sizeof( DWORD );

		*shutBit32 = 0x0FFFFFF0 | bpb->media;
		*errBit32 = MS_EOC32;
	}

	return FAT_SUCCESS;
}

int clear_fat( DISK_OPERATIONS* disk, FAT_BPB* bpb )
{
	UINT32	i, end;
	UINT32	FATSize;
	SECTOR	fatSector;
	BYTE	sector[MAX_SECTOR_SIZE];

	ZeroMemory( sector, sizeof( sector ) );
	fatSector = bpb->reservedSectorCount;

	if( bpb->FATSize16 != 0 )
		FATSize = bpb->FATSize16;
	else
		FATSize = bpb->BPB32.FATSize32;

	end = fatSector + ( FATSize * bpb->numberOfFATs );

	fill_reserved_fat( bpb, sector );
	disk->write_sector( disk, fatSector, sector );

	ZeroMemory( sector, sizeof( sector ) );

	for( i = fatSector + 1; i < end; i++ )
		disk->write_sector( disk, i, sector );

	return FAT_SUCCESS;
}

int create_root( DISK_OPERATIONS* disk, FAT_BPB* bpb )
{
	BYTE	sector[MAX_SECTOR_SIZE];
	SECTOR	rootSector = 0;
	FAT_DIR_ENTRY*	entry;

	ZeroMemory( sector, MAX_SECTOR_SIZE );
	entry = ( FAT_DIR_ENTRY* )sector;

	memcpy( entry->name, VOLUME_LABEL, 11 );
	entry->attribute = ATTR_VOLUME_ID;

	/* Mark as no more directory is in here */
	entry++;
	entry->name[0] = DIR_ENTRY_NO_MORE;

	if( get_fat_type( bpb ) == FAT32 )
	{
		/* Not implemented yet */
	}
	else
		rootSector = bpb->reservedSectorCount + ( bpb->numberOfFATs * bpb->FATSize16 );

	disk->write_sector( disk, rootSector, sector );

	return FAT_SUCCESS;
}

int get_fat_sector( FAT_FILESYSTEM* fs, SECTOR cluster, SECTOR* fatSector, DWORD* fatEntryOffset )
{
	DWORD	fatOffset;

	switch( fs->FATType )
	{
	case FAT32:
		fatOffset = cluster * 4;
		break;
	case FAT16:
		fatOffset = cluster * 2;
		break;
	case FAT12:
		fatOffset = cluster + ( cluster / 2 );
		break;
	default:
		WARNING( "Illegal file system type\n" );
		fatOffset = 0;
		break;
	}

	*fatSector		= fs->bpb.reservedSectorCount + ( fatOffset / fs->bpb.bytesPerSector );
	*fatEntryOffset	= fatOffset % fs->bpb.bytesPerSector;

	return FAT_SUCCESS;
}

int prepare_fat_sector( FAT_FILESYSTEM* fs, SECTOR cluster, SECTOR* fatSector, DWORD* fatEntryOffset, BYTE* sector )
{
	get_fat_sector( fs, cluster, fatSector, fatEntryOffset );
	fs->disk->read_sector( fs->disk, *fatSector, sector );

	if( fs->FATType == FAT12 && *fatEntryOffset == fs->bpb.bytesPerSector - 1 )
	{
		fs->disk->read_sector( fs->disk, *fatSector + 1, &sector[fs->bpb.bytesPerSector] );
		return 1;
	}

	return 0;
}

/* Read a FAT entry from FAT Table */
DWORD get_fat( FAT_FILESYSTEM* fs, SECTOR cluster )
{
	BYTE	sector[MAX_SECTOR_SIZE * 2];
	SECTOR	fatSector;
	DWORD	fatEntryOffset;

	prepare_fat_sector( fs, cluster, &fatSector, &fatEntryOffset, sector );

	switch( fs->FATType )
	{
	case FAT32:
		return ( *( ( DWORD* )&sector[fatEntryOffset] ) ) & 0xFFFFFFF;
	case FAT16:
		return ( DWORD )( *( ( WORD *)&sector[fatEntryOffset] ) );
	case FAT12:
		if( cluster & 1 )	/* Cluster number is ODD	*/
			return ( DWORD )( *( ( WORD *)&sector[fatEntryOffset] ) >> 4 );
		else				/* Cluster number is EVEN	*/
			return ( DWORD )( *( ( WORD *)&sector[fatEntryOffset] ) & 0xFFF );
	}

	return FAT_ERROR;
}

/* Write a FAT entry to FAT Table */
int set_fat( FAT_FILESYSTEM* fs, SECTOR cluster, DWORD value )
{
	BYTE	sector[MAX_SECTOR_SIZE * 2];
	SECTOR	fatSector;
	DWORD	fatEntryOffset;
	int		result;

	result = prepare_fat_sector( fs, cluster, &fatSector, &fatEntryOffset, sector );

	switch( fs->FATType )
	{
	case FAT32:
		value &= 0x0FFFFFFF;
		*( ( DWORD* )&sector[fatEntryOffset] ) &= 0xF0000000;
		*( ( DWORD* )&sector[fatEntryOffset] ) |= value;
		break;
	case FAT16:
		*( ( WORD* )&sector[fatEntryOffset] ) = ( WORD )value;
		break;
	case FAT12:
		if( cluster & 1 )
		{
			value <<= 4;
			*( ( WORD* )&sector[fatEntryOffset] ) &= 0x000F;
		}
		else
		{
			value &= 0x0FFF;
			*( ( WORD* )&sector[fatEntryOffset] ) &= 0xF000;
		}
		*( ( WORD* )&sector[fatEntryOffset] ) |= ( WORD )value;
		break;
	}

	fs->disk->write_sector( fs->disk, fatSector, sector );
	if( result )
		fs->disk->write_sector( fs->disk, fatSector + 1, &sector[fs->bpb.bytesPerSector] );

	return FAT_SUCCESS;
}

/******************************************************************************/
/* Format disk as a specified file system                                     */
/******************************************************************************/
int fat_format( DISK_OPERATIONS* disk, BYTE FATType )
{
	FAT_BPB bpb;

	if( fill_bpb( &bpb, FATType, disk->numberOfSectors, disk->bytesPerSector ) != FAT_SUCCESS ) // bpb 초기화
		return FAT_ERROR;

	disk->write_sector( disk, 0, &bpb );

	PRINTF( "bytes per sector       : %u\n", bpb.bytesPerSector );
	PRINTF( "sectors per cluster    : %u\n", bpb.sectorsPerCluster );
	PRINTF( "number of FATs         : %u\n", bpb.numberOfFATs );
	PRINTF( "root entry count       : %u\n", bpb.rootEntryCount );
	PRINTF( "total sectors          : %u\n", ( bpb.totalSectors ? bpb.totalSectors : bpb.totalSectors32 ) );
	PRINTF( "\n" );

	clear_fat( disk, &bpb ); // FAT 테이블 초기화
	create_root( disk, &bpb ); // root 디렉토리 생성 + 초기화

	return FAT_SUCCESS;
}

int validate_bpb( FAT_BPB* bpb )
{
	int FATType;

	if( !( bpb->jmpBoot[0] == 0xEB && bpb->jmpBoot[2] == 0x90 ) &&
		!( bpb->jmpBoot[0] == 0xE9 ) )
		return FAT_ERROR;

	FATType = get_fat_type( bpb );

	if( FATType < 0 )
		return FAT_ERROR;

	return FAT_SUCCESS;
}

/* when FAT type is FAT12 or FAT16 */
int read_root_sector( FAT_FILESYSTEM* fs, SECTOR sectorNumber, BYTE* sector )
{
	SECTOR	rootSector;

	rootSector = fs->bpb.reservedSectorCount + ( fs->bpb.numberOfFATs * fs->bpb.FATSize16 );

	return fs->disk->read_sector( fs->disk, rootSector + sectorNumber, sector );
}

int write_root_sector( FAT_FILESYSTEM* fs, SECTOR sectorNumber, const BYTE* sector )
{
	SECTOR	rootSector;

	rootSector = fs->bpb.reservedSectorCount + ( fs->bpb.numberOfFATs * fs->bpb.FATSize16 );

	return fs->disk->write_sector( fs->disk, rootSector + sectorNumber, sector );
}

/* Translate logical cluster and sector numbers to a physical sector number */
SECTOR	calc_physical_sector( FAT_FILESYSTEM* fs, SECTOR clusterNumber, SECTOR sectorNumber )
{
	SECTOR	firstDataSector;
	SECTOR	firstSectorOfCluster;
	SECTOR	rootDirSectors;

	rootDirSectors = ( ( fs->bpb.rootEntryCount * 32 ) + ( fs->bpb.bytesPerSector - 1 ) ) / fs->bpb.bytesPerSector ;
	firstDataSector = fs->bpb.reservedSectorCount + ( fs->bpb.numberOfFATs * fs->FATSize ) + rootDirSectors;
	firstSectorOfCluster = ( ( clusterNumber - 2 ) * fs->bpb.sectorsPerCluster ) + firstDataSector;

	return firstSectorOfCluster + sectorNumber;
}

int read_data_sector( FAT_FILESYSTEM* fs, SECTOR clusterNumber, SECTOR sectorNumber, BYTE* sector )
{
	return fs->disk->read_sector( fs->disk, calc_physical_sector( fs, clusterNumber, sectorNumber ), sector );
}

int write_data_sector( FAT_FILESYSTEM* fs, SECTOR clusterNumber, SECTOR sectorNumber, const BYTE* sector )
{
	return fs->disk->write_sector( fs->disk, calc_physical_sector( fs, clusterNumber, sectorNumber ), sector );
}

/* search free clusters from FAT and add to free cluster list */
int search_free_clusters( FAT_FILESYSTEM* fs )
{
	UINT32	totalSectors, dataSector, rootSector, countOfClusters, FATSize;
	UINT32	i, cluster;

	rootSector = ( ( fs->bpb.rootEntryCount * 32 ) + ( fs->bpb.bytesPerSector - 1 ) ) / fs->bpb.bytesPerSector;

	if( fs->bpb.FATSize16 != 0 )
		FATSize = fs->bpb.FATSize16;
	else
		FATSize = fs->bpb.BPB32.FATSize32;

	if( fs->bpb.totalSectors != 0 )
		totalSectors = fs->bpb.totalSectors;
	else
		totalSectors = fs->bpb.totalSectors32;

	dataSector = totalSectors - ( fs->bpb.reservedSectorCount + ( fs->bpb.numberOfFATs * FATSize ) + rootSector );
	countOfClusters = dataSector / fs->bpb.sectorsPerCluster;

	for( i = 2; i < countOfClusters; i++ )
	{
		cluster = get_fat( fs, i );
		if( cluster == FREE_CLUSTER )
			add_free_cluster( fs, i );
	}

	return FAT_SUCCESS;
}

int fat_read_superblock( FAT_FILESYSTEM* fs, FAT_NODE* root )
{
	INT		result;
	BYTE	sector[MAX_SECTOR_SIZE];

	if( fs == NULL || fs->disk == NULL )
	{
		WARNING( "DISK_OPERATIONS : %p\nFAT_FILESYSTEM : %p\n", fs, fs->disk );
		return FAT_ERROR;
	}

	if( fs->disk->read_sector( fs->disk, 0, &fs->bpb ) )
		return FAT_ERROR;
	result = validate_bpb( &fs->bpb );

	if( result )
	{
		WARNING( "BPB validation is failed\n" );
		return FAT_ERROR;
	}

	fs->FATType = get_fat_type( &fs->bpb );
	if( fs->FATType > FAT32 )
		return FAT_ERROR;

	if( read_root_sector( fs, 0, sector ) )
		return FAT_ERROR;

	ZeroMemory( root, sizeof( FAT_NODE ) );
	memcpy( &root->entry, sector, sizeof( FAT_DIR_ENTRY ) );
	root->fs = fs;

	fs->EOCMark = get_fat( fs, 1 );
	if( fs->FATType == 2 )
	{
		if( fs->EOCMark & SHUT_BIT_MASK32 )
			WARNING( "disk drive did not dismount correctly\n" );
		if( fs->EOCMark & ERR_BIT_MASK32 )
			WARNING( "disk drive has error\n" );
	}
	else
	{
		if( fs->FATType == 1)
		{
			if( fs->EOCMark & SHUT_BIT_MASK16 )
				PRINTF( "disk drive did not dismounted\n" );
			if( fs->EOCMark & ERR_BIT_MASK16 )
				PRINTF( "disk drive has error\n" );
		}
	}

	if( fs->bpb.FATSize16 != 0 )
		fs->FATSize = fs->bpb.FATSize16;
	else
		fs->FATSize = fs->bpb.BPB32.FATSize32;

	init_cluster_list( &fs->freeClusterList );
	search_free_clusters( fs );

	memset( root->entry.name, 0x20, 11 );
	return FAT_SUCCESS;
}

/******************************************************************************/
/* On unmount file system                                                     */
/******************************************************************************/
void fat_umount( FAT_FILESYSTEM* fs )
{
	release_cluster_list( &fs->freeClusterList );
}

int read_dir_from_sector( FAT_FILESYSTEM* fs, FAT_ENTRY_LOCATION* location, BYTE* sector, FAT_NODE_ADD adder, void* list )
{
	UINT		i, entriesPerSector;
	FAT_DIR_ENTRY*	dir;
	FAT_NODE	node;

	entriesPerSector = fs->bpb.bytesPerSector / sizeof( FAT_DIR_ENTRY );
	dir = ( FAT_DIR_ENTRY* )sector;

	for( i = 0; i < entriesPerSector; i++ )
	{
		if( dir->name[0] == DIR_ENTRY_FREE )
			;
		else if( dir->name[0] == DIR_ENTRY_NO_MORE )
			break;
		else if( !( dir->attribute & ATTR_VOLUME_ID ) )
		{
			node.fs = fs;
			node.location = *location;
			node.location.number = i;
			node.entry = *dir;
			adder( list, &node );		/* call the callback function that adds entries to list */
		}

		dir++;
	}

	return ( i == entriesPerSector ? 0 : -1 );
}

DWORD get_MS_EOC( BYTE FATType )
{
	switch( FATType )
	{
	case FAT12:
		return MS_EOC12;
	case FAT16:
		return MS_EOC16;
	case FAT32:
		return MS_EOC32;
	}

	WARNING( "Incorrect FATType(%u)\n", FATType );
	return -1;
}

int is_EOC( BYTE FATType, SECTOR clusterNumber )
{
	switch( FATType )
	{
	case FAT12:
		if( EOC12 <= ( clusterNumber & 0xFFF ) )
			return -1;

		break;
	case FAT16:
		if( EOC16 <= ( clusterNumber & 0xFFFF ) )
			return -1;

		break;
	case FAT32:
		if( EOC32 <= ( clusterNumber & 0x0FFFFFFF ) )
			return -1;
		break;
	default:
		WARNING( "Incorrect FATType(%u)\n", FATType );
	}

	return 0;
}

/******************************************************************************/
/* Read all entries in the current directory                                  */
/******************************************************************************/
int fat_read_dir( FAT_NODE* dir, FAT_NODE_ADD adder, void* list )
{
	BYTE	sector[MAX_SECTOR_SIZE];
	SECTOR	i, j, rootEntryCount;
	FAT_ENTRY_LOCATION location;

	if( IS_POINT_ROOT_ENTRY( dir->entry ) && ( dir->fs->FATType == FAT12 || dir->fs->FATType == FAT16 ) )
	{
		if( dir->fs->FATType != FAT32 )
			rootEntryCount = dir->fs->bpb.rootEntryCount;

		for( i = 0; i < rootEntryCount; i++ )
		{
			read_root_sector( dir->fs, i, sector );
			location.cluster = 0;
			location.sector = i;
			location.number = 0;
			if( read_dir_from_sector( dir->fs, &location, sector, adder, list ) )
				break;
		}
	}
	else
	{
		i = GET_FIRST_CLUSTER( dir->entry );
		do
		{
			for( j = 0; j < dir->fs->bpb.sectorsPerCluster; j++ )
			{
				read_data_sector( dir->fs, i, j, sector );
				location.cluster = i;
				location.sector = j;
				location.number = 0;

				if( read_dir_from_sector( dir->fs, &location, sector, adder, list ) )
					break;
			}
			i = get_fat( dir->fs, i );
		} while( !is_EOC( dir->fs->FATType, i ) && i != 0 );
	}

	return FAT_SUCCESS;
}

int add_free_cluster( FAT_FILESYSTEM* fs, SECTOR cluster )
{
	return push_cluster( &fs->freeClusterList, cluster );
}

SECTOR alloc_free_cluster( FAT_FILESYSTEM* fs )
{
	SECTOR	cluster;

	if( pop_cluster( &fs->freeClusterList, &cluster ) == FAT_ERROR )
		return 0;

	return cluster;
}

SECTOR span_cluster_chain( FAT_FILESYSTEM* fs, SECTOR clusterNumber )
{
	UINT32	nextCluster;

	nextCluster = alloc_free_cluster( fs );

	if( nextCluster )
	{
		set_fat( fs, clusterNumber, nextCluster );
		set_fat( fs, nextCluster, get_MS_EOC( fs->FATType ) );
	}

	return nextCluster;
}

int find_entry_at_sector( const BYTE* sector, const BYTE* formattedName, UINT32 begin, UINT32 last, UINT32* number )
{
	UINT32	i;
	const FAT_DIR_ENTRY*	entry = ( FAT_DIR_ENTRY* )sector;

	for( i = begin; i <= last; i++ )
	{
		if( formattedName == NULL )
		{
			if( entry[i].name[0] != DIR_ENTRY_FREE && entry[i].name[0] != DIR_ENTRY_NO_MORE )
			{
				*number = i;
				return FAT_SUCCESS;
			}
		}
		else
		{
			if( ( formattedName[0] == DIR_ENTRY_FREE || formattedName[0] == DIR_ENTRY_NO_MORE ) &&
				( formattedName[0] == entry[i].name[0] ) )
			{
				*number = i;
				return FAT_SUCCESS;
			}

			if( memcmp( entry[i].name, formattedName, MAX_ENTRY_NAME_LENGTH ) == 0 )
			{
				*number = i;
				return FAT_SUCCESS;
			}
		}

		if( entry[i].name[0] == DIR_ENTRY_NO_MORE )
		{
			*number = i;
			return -2;
		}
	}

	*number = i;
	return -1;
}

int find_entry_on_root( FAT_FILESYSTEM* fs, const FAT_ENTRY_LOCATION* first, const BYTE* formattedName, FAT_NODE* ret )
{
	BYTE	sector[MAX_SECTOR_SIZE];
	UINT32	i, number;
	UINT32	lastSector;
	UINT32	entriesPerSector, lastEntry;
	INT32	begin = first->number;
	INT32	result;
	FAT_DIR_ENTRY*	entry;

	entriesPerSector	= fs->bpb.bytesPerSector / sizeof( FAT_DIR_ENTRY );
	lastEntry			= entriesPerSector - 1;
	lastSector			= fs->bpb.rootEntryCount / entriesPerSector;

	for( i = first->sector; i <= lastSector; i++ )
	{
		read_root_sector( fs, i, sector );
		entry = ( FAT_DIR_ENTRY* )sector;

		result = find_entry_at_sector( sector, formattedName, begin, lastEntry, &number );
		begin = 0;

		if( result == -1 )
			continue;
		else
		{
			if( result == -2 )
				return FAT_ERROR;
			else
			{
				memcpy( &ret->entry, &entry[number], sizeof( FAT_DIR_ENTRY ) );
				ret->location.cluster	= 0;
				ret->location.sector	= i;
				ret->location.number	= number;

				ret->fs = fs;
			}

			return FAT_SUCCESS;
		}
	}

	return FAT_ERROR;
}

int find_entry_on_data( FAT_FILESYSTEM* fs, const FAT_ENTRY_LOCATION* first, const BYTE* formattedName, FAT_NODE* ret )
{
	BYTE	sector[MAX_SECTOR_SIZE];
	UINT32	i, number;
	UINT32	entriesPerSector, lastEntry;
	UINT32	currentCluster;
	INT32	begin = first->number;
	INT32	result;
	FAT_DIR_ENTRY*	entry;

	currentCluster		= first->cluster;
	entriesPerSector	= fs->bpb.bytesPerSector / sizeof( FAT_DIR_ENTRY );
	lastEntry			= entriesPerSector - 1;

	while( -1 )
	{
		UINT32	nextCluster;

		for( i = first->sector; i < fs->bpb.sectorsPerCluster; i++ )
		{
			read_data_sector( fs, currentCluster, i, sector );
			entry = ( FAT_DIR_ENTRY* )sector;

			result = find_entry_at_sector( sector, formattedName, begin, lastEntry, &number );
			begin = 0;

			if( result == -1 )
				continue;
			else
			{
				if( result == -2 )
					return FAT_ERROR;
				else
				{
					memcpy( &ret->entry, &entry[number], sizeof( FAT_DIR_ENTRY ) );

					ret->location.cluster	= currentCluster;
					ret->location.sector	= i;
					ret->location.number	= number;

					ret->fs = fs;
				}

				return FAT_SUCCESS;
			}
		}

		nextCluster = get_fat( fs, currentCluster );

		if( is_EOC( fs->FATType, nextCluster ) )
			break;
		else if( nextCluster == 0)
			break;

		currentCluster = nextCluster;
	}

	return FAT_ERROR;
}
// lookup_entry( parent->fs, &first, name, retEntry ) =
/* entryName = NULL -> Find any valid entry */
int lookup_entry( FAT_FILESYSTEM* fs, const FAT_ENTRY_LOCATION* first, const BYTE* entryName, FAT_NODE* ret )
{
	if( first->cluster == 0 && ( fs->FATType == FAT12 || fs->FATType == FAT16 ) )
		return find_entry_on_root( fs, first, entryName, ret );
	else
		return find_entry_on_data( fs, first, entryName, ret );
}

int set_entry( FAT_FILESYSTEM* fs, const FAT_ENTRY_LOCATION* location, const FAT_DIR_ENTRY* value )
{
	BYTE	sector[MAX_SECTOR_SIZE];
	FAT_DIR_ENTRY*	entry;

	if( location->cluster == 0 && ( fs->FATType == FAT12 || fs->FATType == FAT16 ) )
	{
		read_root_sector( fs, location->sector, sector );

		entry = ( FAT_DIR_ENTRY* )sector;
		entry[location->number] = *value;

		write_root_sector( fs, location->sector, sector );
	}
	else
	{
		read_data_sector( fs, location->cluster, location->sector, sector );

		entry = ( FAT_DIR_ENTRY* )sector;
		entry[location->number] = *value;

		write_data_sector( fs, location->cluster, location->sector, sector );
	}

	return FAT_ERROR;
}

int insert_entry( const FAT_NODE* parent, FAT_NODE* newEntry, BYTE overwrite )
{
	FAT_ENTRY_LOCATION	begin;
	FAT_NODE			entryNoMore;
	BYTE				entryName[2] = { 0, };

	begin.cluster = GET_FIRST_CLUSTER( parent->entry );
	begin.sector = 0;
	begin.number = 0;

	if( !( IS_POINT_ROOT_ENTRY( parent->entry ) && ( parent->fs->FATType == FAT12 || parent->fs->FATType == FAT16 ) ) && overwrite )
	{
		begin.number = 0;

		set_entry( parent->fs, &begin, &newEntry->entry );
		newEntry->location = begin;

		/* End of entries */
		begin.number = 1;
		ZeroMemory( &entryNoMore, sizeof( FAT_NODE ) );
		entryNoMore.entry.name[0] = DIR_ENTRY_NO_MORE;
		set_entry( parent->fs, &begin, &entryNoMore.entry );

		return FAT_SUCCESS;
	}

	/* find empty(unused) entry */
	entryName[0] = DIR_ENTRY_FREE;
	if( lookup_entry( parent->fs, &begin, entryName, &entryNoMore ) == FAT_SUCCESS )
	{
		set_entry( parent->fs, &entryNoMore.location, &newEntry->entry );
		newEntry->location = entryNoMore.location;
	}
	else
	{
		if( IS_POINT_ROOT_ENTRY( parent->entry ) && ( parent->fs->FATType == FAT12 || parent->fs->FATType == FAT16 ) )
		{
			UINT32	rootEntryCount = newEntry->location.sector * ( parent->fs->bpb.bytesPerSector / sizeof( FAT_DIR_ENTRY ) ) + newEntry->location.number;
			if( rootEntryCount >= parent->fs->bpb.rootEntryCount )
			{
				WARNING( "Cannot insert entry into the root entry\n" );
				return FAT_ERROR;
			}
		}

		/* add new entry to end */
		entryName[0] = DIR_ENTRY_NO_MORE;
		if( lookup_entry( parent->fs, &begin, entryName, &entryNoMore ) == FAT_ERROR )
			return FAT_ERROR;

		set_entry( parent->fs, &entryNoMore.location, &newEntry->entry );
		newEntry->location = entryNoMore.location;
		entryNoMore.location.number++;

		if( entryNoMore.location.number == ( parent->fs->bpb.bytesPerSector / sizeof( FAT_DIR_ENTRY ) ) )
		{
			entryNoMore.location.sector++;
			entryNoMore.location.number = 0;

			if( entryNoMore.location.sector == parent->fs->bpb.sectorsPerCluster )
			{
				if( !( IS_POINT_ROOT_ENTRY( parent->entry ) && ( parent->fs->FATType == FAT12 || parent->fs->FATType == FAT16 ) ) )
				{
					entryNoMore.location.cluster = span_cluster_chain( parent->fs, entryNoMore.location.cluster );

					if( entryNoMore.location.cluster == 0 )
					{
						NO_MORE_CLUSER();
						return FAT_ERROR;
					}
					entryNoMore.location.sector = 0;
				}
			}
		}

		/* End of entries */
		set_entry( parent->fs, &entryNoMore.location, &entryNoMore.entry );
	}

	return FAT_SUCCESS;
}

void upper_string( char* str, int length )
{
	while( *str && length-- > 0 )
	{
		*str = toupper( *str );
		str++;
	}
}

int format_name( FAT_FILESYSTEM* fs, char* name )
{
	UINT32	i, length;
	UINT32	extender = 0, nameLength = 0;
	UINT32	extenderCurrent = 8;
	BYTE	regularName[MAX_ENTRY_NAME_LENGTH];

	memset( regularName, 0x20, sizeof( regularName ) );
	length = strlen( name );

	if( strncmp( name, "..", 2 ) == 0 )
	{
		memcpy( name, "..         ", 11 );
		return FAT_SUCCESS;
	}
	else if( strncmp( name, ".", 1 ) == 0 )
	{
		memcpy( name, ".          ", 11 );
		return FAT_SUCCESS;
	}

	if( fs->FATType == FAT32 )
	{
	}
	else
	{
		upper_string( name, MAX_ENTRY_NAME_LENGTH );

		for( i = 0; i < length; i++ )
		{
			if( name[i] != '.' && !isdigit( name[i] ) && !isalpha( name[i] ) )
				return FAT_ERROR;

			if( name[i] == '.' )
			{
				if( extender )
					return FAT_ERROR;		/* dot character is allowed only once */
				extender = 1;
			}
			else if( isdigit( name[i] ) || isalpha( name[i] ) )
			{
				if( extender )
					regularName[extenderCurrent++] = name[i];
				else
					regularName[nameLength++] = name[i];
			}
			else
				return FAT_ERROR;			/* non-ascii name is not allowed */
		}

		if( nameLength > 8 || nameLength == 0 || extenderCurrent > 11 )
			return FAT_ERROR;
	}

	memcpy( name, regularName, sizeof( regularName ) );
	return FAT_SUCCESS;
}

/******************************************************************************/
/* Create new directory                                                       */
/******************************************************************************/
int fat_mkdir( const FAT_NODE* parent, const char* entryName, FAT_NODE* ret )
{
	FAT_NODE		dotNode, dotdotNode;
	DWORD			firstCluster;
	BYTE			name[MAX_NAME_LENGTH];
	int				result;

	strncpy( name, entryName, MAX_NAME_LENGTH );

	if( format_name( parent->fs, name ) )
		return FAT_ERROR;

	/* newEntry */
	ZeroMemory( ret, sizeof( FAT_NODE ) );
	memcpy( ret->entry.name, name, MAX_ENTRY_NAME_LENGTH );
	ret->entry.attribute = ATTR_DIRECTORY;
	firstCluster = alloc_free_cluster( parent->fs );

	if( firstCluster == 0 )
	{
		NO_MORE_CLUSER();
		return FAT_ERROR;
	}
	set_fat( parent->fs, firstCluster, get_MS_EOC( parent->fs->FATType ) );

	SET_FIRST_CLUSTER( ret->entry, firstCluster );
	result = insert_entry( parent, ret, 0 );
	if( result )
		return FAT_ERROR;

	ret->fs = parent->fs;

	/* dotEntry */
	ZeroMemory( &dotNode, sizeof( FAT_NODE ) );
	memset( dotNode.entry.name, 0x20, 11 );
	dotNode.entry.name[0] = '.';
	dotNode.entry.attribute = ATTR_DIRECTORY;
	SET_FIRST_CLUSTER( dotNode.entry, firstCluster );
	insert_entry( ret, &dotNode, DIR_ENTRY_OVERWRITE );

	/* dotdotEntry */
	ZeroMemory( &dotdotNode, sizeof( FAT_NODE ) );
	memset( dotdotNode.entry.name, 0x20, 11 );
	dotdotNode.entry.name[0] = '.';
	dotdotNode.entry.name[1] = '.';
	dotdotNode.entry.attribute = ATTR_DIRECTORY;
	SET_FIRST_CLUSTER( dotdotNode.entry, GET_FIRST_CLUSTER( parent->entry ) );
	insert_entry( ret, &dotdotNode, 0 );

	return FAT_SUCCESS;
}

int free_cluster_chain( FAT_FILESYSTEM* fs, DWORD firstCluster )
{
	DWORD	currentCluster = firstCluster;
	DWORD	nextCluster;

	while( !is_EOC( fs->FATType, currentCluster ) && currentCluster != FREE_CLUSTER )
	{
		nextCluster = get_fat( fs, currentCluster );
		set_fat( fs, currentCluster, FREE_CLUSTER );
		add_free_cluster( fs, currentCluster );
		currentCluster = nextCluster;
	}

	return FAT_SUCCESS;
}

int has_sub_entries( FAT_FILESYSTEM* fs, const FAT_DIR_ENTRY* entry )
{
	FAT_ENTRY_LOCATION	begin;
	FAT_NODE			subEntry;

	begin = get_entry_location( entry );
	begin.number = 2;		/* Ignore the '.' and '..' entries */

	if( !lookup_entry( fs, &begin, NULL, &subEntry ) )
		return FAT_ERROR;

	return FAT_SUCCESS;
}

/******************************************************************************/
/* Remove directory                                                           */
/******************************************************************************/
int fat_rmdir( FAT_NODE* dir )
{
	if( has_sub_entries( dir->fs, &dir->entry ) )
		return FAT_ERROR;

	if( !( dir->entry.attribute & ATTR_DIRECTORY ) )		/* Is directory? */
		return FAT_ERROR;

	dir->entry.name[0] = DIR_ENTRY_FREE;
	set_entry( dir->fs, &dir->location, &dir->entry );
	free_cluster_chain( dir->fs, GET_FIRST_CLUSTER( dir->entry ) );

	return FAT_SUCCESS;
}

/******************************************************************************/
/* Lookup entry(file or directory)                                            */
/******************************************************************************/
int fat_lookup( FAT_NODE* parent, const char* entryName, FAT_NODE* retEntry )
{
	FAT_ENTRY_LOCATION	begin;
	BYTE	formattedName[MAX_NAME_LENGTH] = { 0, };

	begin.cluster = GET_FIRST_CLUSTER( parent->entry );
	begin.sector = 0;
	begin.number = 0;

	strncpy( formattedName, entryName, MAX_NAME_LENGTH );

	if( format_name( parent->fs, formattedName ) )
		return FAT_ERROR;

	if( IS_POINT_ROOT_ENTRY( parent->entry ) )
		begin.cluster = 0;

	return lookup_entry( parent->fs, &begin, formattedName, retEntry );
}

/******************************************************************************/
/* Create new file                                                            */
/******************************************************************************/
int fat_create( FAT_NODE* parent, const char* entryName, FAT_NODE* retEntry )
{
	FAT_ENTRY_LOCATION	first;//location of cluster sector number
	BYTE				name[MAX_NAME_LENGTH] = { 0, };
	int					result;

	strncpy( name, entryName, MAX_NAME_LENGTH );

	if( format_name( parent->fs, name ) )
		return FAT_ERROR;

	/* newEntry */
	ZeroMemory( retEntry, sizeof( FAT_NODE ) );
	memcpy( retEntry->entry.name, name, MAX_ENTRY_NAME_LENGTH );

	first.cluster = parent->entry.firstClusterLO;
	first.sector = 0;
	first.number = 0;
	if( lookup_entry( parent->fs, &first, name, retEntry ) == FAT_SUCCESS )
		return FAT_ERROR;

	retEntry->fs = parent->fs;
	result = insert_entry( parent, retEntry, 0 );
	if( result )
		return FAT_ERROR;

	return FAT_SUCCESS;
}

/******************************************************************************/
/* Read file                                                                  */
/******************************************************************************/
int fat_read( FAT_NODE* file, unsigned long offset, unsigned long length, char* buffer )
{
	BYTE	sector[MAX_SECTOR_SIZE];
	DWORD	currentOffset, currentCluster, clusterSeq = 0;
	DWORD	clusterNumber, sectorNumber, sectorOffset;
	DWORD	readEnd;
	DWORD	clusterSize, clusterOffset = 0;

	currentCluster = GET_FIRST_CLUSTER( file->entry );
	readEnd = MIN( offset + length, file->entry.fileSize );

	currentOffset = offset;

	clusterSize = ( file->fs->bpb.bytesPerSector * file->fs->bpb.sectorsPerCluster );
	clusterOffset = clusterSize;
	while( offset > clusterOffset )
	{
		currentCluster = get_fat( file->fs, currentCluster );
		clusterOffset += clusterSize;
		clusterSeq++;
	}

	while( currentOffset < readEnd )
	{
		DWORD	copyLength;

		clusterNumber	= currentOffset / ( file->fs->bpb.bytesPerSector * file->fs->bpb.sectorsPerCluster );
		if( clusterSeq != clusterNumber )
		{
			clusterSeq++;
			currentCluster = get_fat( file->fs, currentCluster );
		}
		sectorNumber	= ( currentOffset / ( file->fs->bpb.bytesPerSector ) ) % file->fs->bpb.sectorsPerCluster;
		sectorOffset	= currentOffset % file->fs->bpb.bytesPerSector;

		if( read_data_sector( file->fs, currentCluster, sectorNumber, sector ) )
			break;

		copyLength = MIN( file->fs->bpb.bytesPerSector - sectorOffset, readEnd - currentOffset );

		memcpy( buffer,
				&sector[sectorOffset],
				copyLength );

		buffer += copyLength;
		currentOffset += copyLength;
	}

	return currentOffset - offset;
}

/******************************************************************************/
/* Write file                                                                 */
/******************************************************************************/
int fat_write( FAT_NODE* file, unsigned long offset, unsigned long length, const char* buffer )
{
	BYTE	sector[MAX_SECTOR_SIZE];
	DWORD	currentOffset, currentCluster, clusterSeq = 0;
	DWORD	clusterNumber, sectorNumber, sectorOffset;
	DWORD	readEnd;
	DWORD	clusterSize;

	currentCluster = GET_FIRST_CLUSTER( file->entry );
	readEnd = offset + length;

	currentOffset = offset;

	clusterSize = ( file->fs->bpb.bytesPerSector * file->fs->bpb.sectorsPerCluster );
	while( offset > clusterSize )
	{
		currentCluster = get_fat( file->fs, currentCluster );
		clusterSize += clusterSize;
		clusterSeq++;
	}

	while( currentOffset < readEnd )
	{
		DWORD	copyLength;

		clusterNumber	= currentOffset / ( file->fs->bpb.bytesPerSector * file->fs->bpb.sectorsPerCluster );

		if( currentCluster == 0 )
		{
			currentCluster = alloc_free_cluster( file->fs );
			if( currentCluster == 0 )
			{
				NO_MORE_CLUSER();
				return FAT_ERROR;
			}

			SET_FIRST_CLUSTER( file->entry, currentCluster );
			set_fat( file->fs, currentCluster, get_MS_EOC( file->fs->FATType ) );
		}

		if( clusterSeq != clusterNumber )
		{
			DWORD nextCluster;
			clusterSeq++;

			nextCluster = get_fat( file->fs, currentCluster );
			if( is_EOC( file->fs->FATType, nextCluster ) )
			{
				nextCluster = span_cluster_chain( file->fs, currentCluster );

				if( nextCluster == 0 )
				{
					NO_MORE_CLUSER();
					break;
				}
			}
			currentCluster = nextCluster;
		}
		sectorNumber	= ( currentOffset / ( file->fs->bpb.bytesPerSector ) ) % file->fs->bpb.sectorsPerCluster;
		sectorOffset	= currentOffset % file->fs->bpb.bytesPerSector;

		copyLength = MIN( file->fs->bpb.bytesPerSector - sectorOffset, readEnd - currentOffset );

		if( copyLength != file->fs->bpb.bytesPerSector )
		{
			if( read_data_sector( file->fs, currentCluster, sectorNumber, sector ) )
				break;
		}

		memcpy( &sector[sectorOffset],
				buffer,
				copyLength );

		if( write_data_sector( file->fs, currentCluster, sectorNumber, sector ) )
			break;

		buffer += copyLength;
		currentOffset += copyLength;
	}

	file->entry.fileSize = MAX( currentOffset, file->entry.fileSize );
	set_entry( file->fs, &file->location, &file->entry );

	return currentOffset - offset;
}

/******************************************************************************/
/* Remove file                                                                */
/******************************************************************************/
int fat_remove( FAT_NODE* file )
{
	if( file->entry.attribute & ATTR_DIRECTORY )		/* Is directory? */
		return FAT_ERROR;

	file->entry.name[0] = DIR_ENTRY_FREE;
	set_entry( file->fs, &file->location, &file->entry );
	free_cluster_chain( file->fs, GET_FIRST_CLUSTER( file->entry ) );

	return FAT_SUCCESS;
}

/******************************************************************************/
/* Disk free spaces                                                           */
/******************************************************************************/
int fat_df( FAT_FILESYSTEM* fs, UINT32* totalSectors, UINT32* usedSectors )
{
	if( fs->bpb.totalSectors != 0 )
		*totalSectors = fs->bpb.totalSectors;
	else
		*totalSectors = fs->bpb.totalSectors32;

	*usedSectors = *totalSectors - ( fs->freeClusterList.count * fs->bpb.sectorsPerCluster );

	return FAT_SUCCESS;
}


/******************************************************************************/
/*                                                                            */
/* Project : FAT12/16 File System                                             */
/* File    : shell.c                                                          */
/* Author  : Kyoungmoon Sun(msg2me@msn.com)                                   */
/* Company : Dankook Univ. Embedded System Lab.                               */
/* Notes   : File System test shell                                           */
/* Date    : 2008/7/2                                                         */
/*                                                                            */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "shell.h"
#include "disksim.h"

#define SECTOR_SIZE				512
#define NUMBER_OF_SECTORS		4096

#define COND_MOUNT				0x01
#define COND_UMOUNT				0x02

typedef struct
{
	char*	name;
	int		( *handler )( int, char** );
	char	conditions;
} COMMAND;

extern void shell_register_filesystem( SHELL_FILESYSTEM* );

void do_shell( void );
void unknown_command( void );
int seperate_string( char* buf, char* ptrs[] );
//명령어처리
int shell_cmd_cd( int argc, char* argv[] );
int shell_cmd_exit( int argc, char* argv[] );
int shell_cmd_mount( int argc, char* argv[] );
int shell_cmd_umount( int argc, char* argv[] );
int shell_cmd_touch( int argc, char* argv[] );
int shell_cmd_fill( int argc, char* argv[] );
int shell_cmd_rm( int argc, char* argv[] );
int shell_cmd_ls( int argc, char* argv[] );
int shell_cmd_format( int argc, char* argv[] );
int shell_cmd_df( int argc, char* argv[] );
int shell_cmd_mkdir( int argc, char* argv[] );
int shell_cmd_rmdir( int argc, char* argv[] );
int shell_cmd_mkdirst( int argc, char* argv[] );
int shell_cmd_cat( int argc, char* argv[] );

static COMMAND g_commands[] =
{
	{ "cd",		shell_cmd_cd,		COND_MOUNT	},
	{ "exit",	shell_cmd_exit,		0			},
	{ "quit",	shell_cmd_exit,		0			},
	{ "mount",	shell_cmd_mount,	COND_UMOUNT	},
	{ "umount",	shell_cmd_umount,	COND_MOUNT	},
	{ "touch",	shell_cmd_touch,	COND_MOUNT	},
	{ "fill",	shell_cmd_fill,		COND_MOUNT	},
	{ "rm",		shell_cmd_rm,		COND_MOUNT	},
	{ "ls",		shell_cmd_ls,		COND_MOUNT	},
	{ "dir",	shell_cmd_ls,		COND_MOUNT	},
	{ "format",	shell_cmd_format,	COND_UMOUNT	},
	{ "df",		shell_cmd_df,		COND_MOUNT	},
	{ "mkdir",	shell_cmd_mkdir,	COND_MOUNT	},
	{ "rmdir",	shell_cmd_rmdir,	COND_MOUNT	},
	{ "mkdirst",shell_cmd_mkdirst,	COND_MOUNT	},
	{ "cat",	shell_cmd_cat,		COND_MOUNT	}
};

static SHELL_FILESYSTEM		g_fs;
static SHELL_FS_OPERATIONS	g_fsOprs;
static SHELL_ENTRY			g_rootDir;
static SHELL_ENTRY			g_currentDir;
static DISK_OPERATIONS		g_disk;

int g_commandsCount = sizeof( g_commands ) / sizeof( COMMAND );
int g_isMounted;

int main( int argc, char* argv[] )
{
	if( disksim_init( NUMBER_OF_SECTORS, SECTOR_SIZE, &g_disk ) < 0 ) //disksim 초기화
	{
		printf( "disk simulator initialization has been failed\n" );
		return -1;
	}

	shell_register_filesystem( &g_fs ); 

	do_shell();

	return 0;
}

int check_conditions( int conditions )
{
	if( conditions & COND_MOUNT && !g_isMounted )
	{
		printf( "file system is not mounted\n" );
		return -1;
	}

	if( conditions & COND_UMOUNT && g_isMounted )
	{
		printf( "file system is already mounted\n" );
		return -1;
	}

	return 0;
}

void do_shell( void )
{
	char buf[1000];
	char command[100];
	char* argv[100];
	int argc;
	int i;

	printf( "%s File system shell\n", g_fs.name );

	while( -1 )
	{
		printf( "[%s/]# ", g_currentDir.name );
		fgets( buf, 1000, stdin );

		argc = seperate_string( buf, argv );

		if( argc == 0 )
			continue;

		for( i = 0; i < g_commandsCount; i++ )
		{
			if( strcmp( g_commands[i].name, argv[0] ) == 0 )
			{
				if( check_conditions( g_commands[i].conditions ) == 0 ) //입력받은 명령어와 비교
					g_commands[i].handler( argc, argv ); //명령어 처리

				break;
			}
		}
		if( argc != 0 && i == g_commandsCount )
			unknown_command();
	}
}

void unknown_command( void ) //알 수 없는 명령어면 모든 명령어 종류 출력
{
	int i;

	printf( " * " );
	for( i = 0; i < g_commandsCount; i++ )
	{
		if( i < g_commandsCount - 1 )
			printf( "%s, ", g_commands[i].name );
		else
			printf( "%s", g_commands[i].name );
	}
	printf( "\n" );
}

int seperate_string( char* buf, char* ptrs[] )
{
	char prev = 0;
	int count = 0;

	while( *buf )
	{
		if( isspace( *buf ) )
			*buf = 0;
		else if( prev == 0 )	/* continually space */
			ptrs[count++] = buf;

		prev = *buf++;
	}

	return count;
}

/******************************************************************************/
/* Shell commands...                                                          */
/******************************************************************************/
int shell_cmd_cd( int argc, char* argv[] )
{
	SHELL_ENTRY	newEntry;
	int			result;
	static SHELL_ENTRY	path[256];
	static int			pathTop = 0;

	path[0] = g_rootDir;

	if( argc > 2 )
	{
		printf( "usage : %s [directory]\n", argv[0] );
		return 0;
	}

	if( argc == 1 )
		pathTop = 0;
	else
	{
		if( strcmp( argv[1], "." ) == 0 )
			return 0;
		else if( strcmp( argv[1], ".." ) == 0 && pathTop > 0 )
			pathTop--;
		else
		{
			result = g_fsOprs.lookup( &g_disk, &g_fsOprs, &g_currentDir, &newEntry, argv[1] ); // entry 찾아서 넘겨줌

			if( result )
			{
				printf( "directory not found\n" );
				return -1;
			}
			else if( !newEntry.isDirectory )
			{
				printf( "%s is not a directory\n", argv[1] );
				return -1;
			}
			path[++pathTop] = newEntry;
		}
	}

	g_currentDir = path[pathTop];

	return 0;
}

int shell_cmd_exit( int argc, char* argv[] )
{
	disksim_uninit( &g_disk );
	_exit( 0 );

	return 0;
}

int shell_cmd_mount( int argc, char* argv[] )
{
	int result;

	if( g_fs.mount == NULL )
	{
		printf( "The mount functions is NULL\n" );
		return 0;
	}

	result = g_fs.mount( &g_disk, &g_fsOprs, &g_rootDir ); //fs.mount --> fat_shell.h
	g_currentDir = g_rootDir; // 현재 디렉토리 = 루트 디렉토리

	if( result < 0 )
	{
		printf( "%s file system mounting has been failed\n", g_fs.name );
		return -1;
	}
	else
	{
		printf( "%s file system has been mounted successfully\n", g_fs.name );
		g_isMounted = 1;
	}

	return 0;
}

int shell_cmd_umount( int argc, char* argv[] )
{
	g_isMounted = 0;

	if( g_fs.umount == NULL )
		return 0;

	g_fs.umount( &g_disk, &g_fsOprs );
	return 0;
}

int shell_cmd_touch( int argc, char* argv[] )
{
	SHELL_ENTRY	entry;
	int			result;

	if( argc < 2 )
	{
		printf( "usage : touch [files...]\n" );
		return 0;
	}

	result = g_fsOprs.fileOprs->create( &g_disk, &g_fsOprs, &g_currentDir, argv[1], &entry );

	if( result )
	{
		printf( "create failed\n" );
		return -1;
	}

	return 0;
}

int shell_cmd_fill( int argc, char* argv[] )
{
	SHELL_ENTRY	entry;
	char*		buffer;
	char*		tmp;
	int			size;
	int			result;

	if( argc != 3 )
	{
		printf( "usage : fill [file] [size]\n" );
		return 0;
	}

	sscanf( argv[2], "%d", &size );

	result = g_fsOprs.fileOprs->create( &g_disk, &g_fsOprs, &g_currentDir, argv[1], &entry );
	if( result )
	{
		printf( "create failed\n" );
		return -1;
	}

	buffer = ( char* )malloc( size + 13 );
	tmp = buffer;
	while( tmp < buffer + size )
	{
		memcpy( tmp, "Can you see? ", 13 );
		tmp += 13;
	}
	g_fsOprs.fileOprs->write( &g_disk, &g_fsOprs, &g_currentDir, &entry, 0, size, buffer );
	free( buffer );

	return 0;
}

int shell_cmd_rm( int argc, char* argv[] )
{
	int i;

	if( argc < 2 )
	{
		printf( "usage : rm [files...]\n" );
		return 0;
	}

	for( i = 1; i < argc; i++ )
	{
		if( g_fsOprs.fileOprs->remove( &g_disk, &g_fsOprs, &g_currentDir, argv[i] ) )
			printf( "cannot remove file\n" );
	}

	return 0;
}

int shell_cmd_ls( int argc, char* argv[] )
{
	SHELL_ENTRY_LIST		list;
	SHELL_ENTRY_LIST_ITEM*	current;

	if( argc > 2 )
	{
		printf( "usage : %s [path]\n", argv[0] );
		return 0;
	}

	init_entry_list( &list );
	if( g_fsOprs.read_dir( &g_disk, &g_fsOprs, &g_currentDir, &list ) )
	{
		printf( "Failed to read_dir\n" );
		return -1;
	}

	current = list.first;

	printf( "[File names] [D] [File sizes]\n" );
	while( current )
	{
		printf( "%-12s  %1d  %12d\n",
				current->entry.name, current->entry.isDirectory, current->entry.size );
		current = current->next;
	}
	printf( "\n" );

	release_entry_list( &list );
	return 0;
}

int shell_cmd_format( int argc, char* argv[] )
{
	int		result;
	char*	param = NULL;

	if( argc >= 2 )
		param = argv[1];

	result = g_fs.format( &g_disk, param );

	if( result < 0 )
	{
		printf( "%s formatting is failed\n", g_fs.name );
		return -1;
	}

	printf( "disk has been formatted successfully\n" );
	return 0;
}

double get_percentage( unsigned int number, unsigned int total )
{
	return ( ( double )number ) / total * 100.;
}

int shell_cmd_df( int argc, char* argv[] )
{
	unsigned int used, total;
	int result;

	g_fsOprs.stat( &g_disk, &g_fsOprs, &total, &used );

	printf( "free sectors : %u(%.2lf%%)\tused sectors : %u(%.2lf%%)\ttotal : %u\n",
			total - used, get_percentage( total - used, g_disk.numberOfSectors ),
		   	used, get_percentage( used, g_disk.numberOfSectors ),
		   	total );

	return 0;
}

int shell_cmd_mkdir( int argc, char* argv[] )
{
	SHELL_ENTRY	entry;
	int result;

	if( argc != 2 )
	{
		printf( "usage : %s [name]\n", argv[0] );
		return 0;
	}

	result = g_fsOprs.mkdir( &g_disk, &g_fsOprs, &g_currentDir, argv[1], &entry );

	if( result )
	{
		printf( "cannot create directory\n" );
		return -1;
	}

	return 0;
}

int shell_cmd_rmdir( int argc, char* argv[] )
{
	int result;

	if( argc != 2 )
	{
		printf( "usage : %s [name]\n", argv[0] );
		return 0;
	}

	result = g_fsOprs.rmdir( &g_disk, &g_fsOprs, &g_currentDir, argv[1] );

	if( result )
	{
		printf( "cannot remove directory\n" );
		return -1;
	}

	return 0;
}

int shell_cmd_mkdirst( int argc, char* argv[] )
{
	SHELL_ENTRY	entry;
	int		result, i, count;
	char	buf[10];

	if( argc != 2 )
	{
		printf( "usage : %s [count]\n", argv[0] );
		return 0;
	}

	sscanf( argv[1], "%d", &count );
	for( i = 0; i < count; i++ )
	{
		sprintf( buf, "%d", i );
		result = g_fsOprs.mkdir( &g_disk, &g_fsOprs, &g_currentDir, buf, &entry );

		if( result )
		{
			printf( "cannot create directory\n" );
			return -1;
		}
	}

	return 0;
}

int shell_cmd_cat( int argc, char* argv[] )
{
	SHELL_ENTRY	entry;
	char		buf[1025] = { 0, };
	int			result;
	unsigned long	offset = 0;

	if( argc != 2 )
	{
		printf( "usage : %s [file name]\n", argv[0] );
		return 0;
	}

	result = g_fsOprs.lookup( &g_disk, &g_fsOprs, &g_currentDir, &entry, argv[1] );
	if( result )
	{
		printf( "%s lookup failed\n", argv[1] );
		return -1;
	}

	while( g_fsOprs.fileOprs->read( &g_disk, &g_fsOprs, &g_currentDir, &entry, offset, 1024, buf ) > 0 )
	{
		printf( "%s", buf );
		offset += 1024;
		memset( buf, 0, sizeof( buf ) );
	}
	printf( "\n" );
}

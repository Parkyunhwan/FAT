/******************************************************************************/
/*                                                                            */
/* Project : FAT12/16 File System                                             */
/* File    : fat_shell.h                                                      */
/* Author  : Kyoungmoon Sun(msg2me@msn.com)                                   */
/* Company : Dankook Univ. Embedded System Lab.                               */
/* Notes   : Adaption layer header between FAT File System and shell          */
/* Date    : 2008/7/2                                                         */
/*                                                                            */
/******************************************************************************/

#ifndef _FAT_SHELL_H_
#define _FAT_SHELL_H_

#include "fat.h"
#include "shell.h"

void shell_register_filesystem( SHELL_FILESYSTEM* );

#endif

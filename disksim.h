/******************************************************************************/
/*                                                                            */
/* Project : FAT12/16 File System                                             */
/* File    : disksim.h                                                        */
/* Author  : Kyoungmoon Sun(msg2me@msn.com)                                   */
/* Company : Dankook Univ. Embedded System Lab.                               */
/* Notes   : Disk simulator header                                            */
/* Date    : 2008/7/2                                                         */
/*                                                                            */
/******************************************************************************/
//disksim = 장치 시뮬레이터
#ifndef _DISKSIM_H_
#define _DISKSIM_H_

#include "common.h"

int disksim_init( SECTOR, unsigned int, DISK_OPERATIONS* );
void disksim_uninit( DISK_OPERATIONS* );

#endif

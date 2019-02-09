/******************************************************************************/
/*                                                                            */
/* Project : FAT12/16 File System                                             */
/* File    : clusterlist.c                                                    */
/* Author  : Kyoungmoon Sun(msg2me@msn.com)                                   */
/* Company : Dankook Univ. Embedded System Lab.                               */
/* Notes   : Cluster list                                                     */
/* Date    : 2008/7/2                                                         */
/*                                                                            */
/******************************************************************************/

#include "common.h"
#include "clusterlist.h"

int	init_cluster_list( CLUSTER_LIST* clusterList ) // 클러스터 리스트 초기화
{
	if( clusterList == NULL )
		return FAT_ERROR;

	ZeroMemory( clusterList, sizeof( CLUSTER_LIST ) ); //memset(clusterList, 0, sizeof(CLUSTER_LIST))

	return FAT_SUCCESS;
}

int	push_cluster( CLUSTER_LIST* clusterList, SECTOR cluster ) 
{
	CLUSTER_LIST_ELEMENT*	entry;

	if( clusterList == NULL )
		return FAT_ERROR;

	if( clusterList->first == NULL ||					/* first push or */
		clusterList->pushOffset == CLUSTERS_PER_ELEMENT )	/* the item is full*/
	{
		entry = ( CLUSTER_LIST_ELEMENT* )malloc( sizeof( CLUSTER_LIST_ELEMENT ) );
		if( entry == NULL )
			return FAT_ERROR;

		entry->next = NULL;

		if( clusterList->first == NULL ) // 리스트의 맨 처음에 삽입
			clusterList->first = entry;
		if( clusterList->last ) // 마지막
			clusterList->last->next = entry;

		clusterList->last = entry;

		clusterList->pushOffset = 0;
	}

	entry = clusterList->last;
	entry->clusters[clusterList->pushOffset++] = cluster;
	clusterList->count++;

	return FAT_SUCCESS;
}

int pop_cluster( CLUSTER_LIST* clusterList, SECTOR* cluster )
{
	CLUSTER_LIST_ELEMENT*	entry;

	if( clusterList == NULL || clusterList->count == 0 )
		return FAT_ERROR;

	entry = clusterList->first;
	if( entry == NULL )
		return FAT_ERROR;

	*cluster = entry->clusters[clusterList->popOffset++];
	clusterList->count--;

	/* the item is empty */
	if( clusterList->popOffset == CLUSTERS_PER_ELEMENT )
	{
		entry = entry->next;
		free( clusterList->first );
		clusterList->first = entry;

		clusterList->popOffset = 0;
	}

	return FAT_SUCCESS;
}

void	release_cluster_list( CLUSTER_LIST* clusterList )
{
	CLUSTER_LIST_ELEMENT* entry;
	CLUSTER_LIST_ELEMENT* nextEntry;

	if( clusterList == NULL )
		return;

	entry = clusterList->first;

	while( entry )
	{
		nextEntry = entry->next;
		free( entry );
		entry = nextEntry;
	}

	clusterList->first = clusterList->last = NULL;
	clusterList->count = 0;
}

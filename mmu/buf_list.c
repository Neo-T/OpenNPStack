#include "port/datatype.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "port/sys_config.h"
#include "errors.h"

#define SYMBOL_GLOBALS
#include "mmu/buf_list.h"
#undef SYMBOL_GLOBALS

//* 不使用结构体设计方式，目的是为了节省内存，同时避免因使用强制对齐方式降低系统执行性能的问题
static void *l_pvaBufNode[BUF_LIST_NUM];
static USHORT l_usaBufSize[BUF_LIST_NUM];
static SHORT l_saFreeBufNode[BUF_LIST_NUM];
static SHORT l_sFreeBufList = -1; 
static HMUTEX l_hMtxMMUBufList;

BOOL buf_list_init(EN_ERROR_CODE *penErrCode)
{
	INT i; 

	//* 清0
	memset(l_pvaBufNode, 0, sizeof(l_pvaBufNode));

	//* 链接	
	for (i = 0; i < BUF_LIST_NUM - 1; i++)	
		l_saFreeBufNode[i] = i + 1; 	
	l_saFreeBufNode[BUF_LIST_NUM - 1] = -1; 
	l_sFreeBufList = 0;

	l_hMtxMMUBufList = os_thread_mutex_init();
	if (INVALID_HMUTEX != l_hMtxMMUBufList)
		return TRUE;

	if(penErrCode)
		*penErrCode = ERRMUTEXINITFAILED;
	return FALSE; 
}

SHORT buf_list_get(EN_ERROR_CODE *penErrCode)
{
	SHORT sRtnNode; 
	os_thread_mutex_lock(l_hMtxMMUBufList);
	{
		if (l_sFreeBufList < 0)
		{
			os_thread_mutex_unlock(l_hMtxMMUBufList);

			if (penErrCode)
				*penErrCode = ERRNOBUFLISTNODE;

			return -1;
		}

		sRtnNode = l_sFreeBufList; 
		l_sFreeBufList = l_saFreeBufNode[l_sFreeBufList]; 
	}
	os_thread_mutex_unlock(l_hMtxMMUBufList);

	return sRtnNode; 
}

SHORT buf_list_get_ext(void *pvData, UINT unDataSize, EN_ERROR_CODE *penErrCode)
{
	SHORT sRtnNode;
	os_thread_mutex_lock(l_hMtxMMUBufList);
	{
		if (l_sFreeBufList < 0)
		{
			os_thread_mutex_unlock(l_hMtxMMUBufList);

			if (penErrCode)
				*penErrCode = ERRNOBUFLISTNODE;

			return -1;
		}

		sRtnNode = l_sFreeBufList;
		l_sFreeBufList = l_saFreeBufNode[l_sFreeBufList];
	}
	os_thread_mutex_unlock(l_hMtxMMUBufList);

	l_pvaBufNode[sRtnNode] = pvData; 
	l_usaBufSize[sRtnNode] = (USHORT)unDataSize; 
	return sRtnNode;
}

void buf_list_attach_data(SHORT sNode, void *pvData, UINT unDataSize)
{
	l_pvaBufNode[sNode] = pvData;
	l_usaBufSize[sNode] = (USHORT)unDataSize;
}

void buf_list_free(SHORT sNode)
{
	os_thread_mutex_lock(l_hMtxMMUBufList);
	{
		l_pvaBufNode[sNode]    = NULL;
		l_usaBufSize[sNode]    = 0; 
		l_saFreeBufNode[sNode] = l_sFreeBufList; 
		l_sFreeBufList         = sNode;
	}
	os_thread_mutex_unlock(l_hMtxMMUBufList);
}

void buf_list_put_head(SHORT *psHead, SHORT sNode)
{
	l_saFreeBufNode[sNode] = *psHead;
	*psHead = sNode;
}

void buf_list_put_tail(SHORT sHead, SHORT sNode)
{
	SHORT sNextNode = sHead; 
	while (sNextNode >= 0)
	{
		if (l_saFreeBufNode[sNextNode] < 0)
		{
			l_saFreeBufNode[sNextNode] = sNode; 
			l_saFreeBufNode[sNode] = -1;
			break; 
		}

		sNextNode = l_saFreeBufNode[sNextNode]; 
	}
}

void *buf_list_get_next_node(SHORT *psNextNode, USHORT *pusDataLen)
{
	if (*psNextNode < 0)
		return NULL;

	void *pvData = NULL;
	*pusDataLen = l_usaBufSize[*psNextNode];
	pvData = l_pvaBufNode[*psNextNode]; 
	*psNextNode = l_saFreeBufNode[*psNextNode];  

	return pvData; 
}

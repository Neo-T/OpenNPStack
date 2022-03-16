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
			*penErrCode = ERRNOBUFLISTNODE;

			return -1;
		}

		sRtnNode = l_sFreeBufList; 
		l_sFreeBufList = l_saFreeBufNode[l_sFreeBufList]; 
	}
	os_thread_mutex_unlock(l_hMtxMMUBufList);

	return sRtnNode; 
}

SHORT buf_list_get_ext(void *pvData, EN_ERROR_CODE *penErrCode)
{
	SHORT sRtnNode;
	os_thread_mutex_lock(l_hMtxMMUBufList);
	{
		if (l_sFreeBufList < 0)
		{
			os_thread_mutex_unlock(l_hMtxMMUBufList);
			*penErrCode = ERRNOBUFLISTNODE;

			return -1;
		}

		sRtnNode = l_sFreeBufList;
		l_sFreeBufList = l_saFreeBufNode[l_sFreeBufList];
	}
	os_thread_mutex_unlock(l_hMtxMMUBufList);

	l_pvaBufNode[sRtnNode] = pvData; 
	return sRtnNode;
}

void buf_list_free(SHORT sNodeIndex)
{
	os_thread_mutex_lock(l_hMtxMMUBufList);
	{
		l_pvaBufNode[sNodeIndex] = NULL;
		l_saFreeBufNode[sNodeIndex] = l_sFreeBufList; 
		l_sFreeBufList = sNodeIndex;
	}
	os_thread_mutex_unlock(l_hMtxMMUBufList);
}
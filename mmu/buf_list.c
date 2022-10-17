/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "port/sys_config.h"
#include "onps_errors.h"

#define SYMBOL_GLOBALS
#include "mmu/buf_list.h"
#undef SYMBOL_GLOBALS

//* 不使用结构体设计方式，目的是为了节省内存，同时避免因使用强制对齐方式降低系统执行性能的问题
static void *l_pvaBufNode[BUF_LIST_NUM];
static USHORT l_usaBufSize[BUF_LIST_NUM];
static SHORT l_saFreeBufNode[BUF_LIST_NUM];
static SHORT l_sFreeBufList = -1; 
//static HMUTEX l_hMtxMMUBufList = INVALID_HMUTEX;

//* 协议栈初始加载时别忘了要先调用这个初始设置函数
BOOL buf_list_init(EN_ONPSERR *penErr)
{
	INT i; 

	//* 清0
	memset(l_pvaBufNode, 0, sizeof(l_pvaBufNode));

	//* 链接	
	for (i = 0; i < BUF_LIST_NUM - 1; i++)	
		l_saFreeBufNode[i] = i + 1; 	
	l_saFreeBufNode[BUF_LIST_NUM - 1] = -1; 
	l_sFreeBufList = 0;

#if 0
	l_hMtxMMUBufList = os_thread_mutex_init();
	if (INVALID_HMUTEX != l_hMtxMMUBufList)
		return TRUE;

	if(penErr)
		*penErr = ERRMUTEXINITFAILED;
	return FALSE; 
#else
    return TRUE; 
#endif
}

void buf_list_uninit(void)
{
    //if (INVALID_HMUTEX != l_hMtxMMUBufList)
    //    os_thread_mutex_uninit(l_hMtxMMUBufList);
}

SHORT buf_list_get(EN_ONPSERR *penErr)
{
	SHORT sRtnNode;

    os_critical_init(); 

	//os_thread_mutex_lock(l_hMtxMMUBufList);
    os_enter_critical(); 
	{
		if (l_sFreeBufList < 0)
		{
            os_exit_critical(); 
			//os_thread_mutex_unlock(l_hMtxMMUBufList);

			if (penErr)
				*penErr = ERRNOBUFLISTNODE;

			return -1;
		}

		sRtnNode = l_sFreeBufList; 
		l_sFreeBufList = l_saFreeBufNode[l_sFreeBufList]; 
	}
	//os_thread_mutex_unlock(l_hMtxMMUBufList);
    os_exit_critical(); 

	return sRtnNode; 
}

SHORT buf_list_get_ext(void *pvData, UINT unDataSize, EN_ONPSERR *penErr)
{
	SHORT sRtnNode;

    os_critical_init();

	//os_thread_mutex_lock(l_hMtxMMUBufList);
    os_enter_critical();
	{
		if (l_sFreeBufList < 0)
		{
			//os_thread_mutex_unlock(l_hMtxMMUBufList);
            os_exit_critical();

			if (penErr)
				*penErr = ERRNOBUFLISTNODE;

			return -1;
		}

		sRtnNode = l_sFreeBufList;
		l_sFreeBufList = l_saFreeBufNode[l_sFreeBufList];
	}
    os_exit_critical();
	//os_thread_mutex_unlock(l_hMtxMMUBufList);

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
    if (sNode < 0)
        return; 

    os_critical_init();

	//os_thread_mutex_lock(l_hMtxMMUBufList);
    os_enter_critical();
	{
		l_pvaBufNode[sNode]    = NULL;
		l_usaBufSize[sNode]    = 0; 
		l_saFreeBufNode[sNode] = l_sFreeBufList; 
		l_sFreeBufList         = sNode;
	}
	//os_thread_mutex_unlock(l_hMtxMMUBufList);
    os_exit_critical();
}

void buf_list_free_head(SHORT *psHead, SHORT sNode)
{
    if (sNode < 0)
        return;

    *psHead = l_saFreeBufNode[sNode]; 
    buf_list_free(sNode); 
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

UINT buf_list_get_len(SHORT sBufListHead)
{
    SHORT sNextNode = sBufListHead;
    USHORT usDataLen; 
    UINT unTotalLen = 0;

    while (NULL != buf_list_get_next_node(&sNextNode, &usDataLen))
        unTotalLen += (UINT)usDataLen;

    return unTotalLen; 
}

//* 将list保存地报文取出、合并后保存到参数pubPacket指向的缓冲区
void buf_list_merge_packet(SHORT sBufListHead, UCHAR *pubPacket)
{
    SHORT sNextNode = sBufListHead; 
    UCHAR *pubData;
    USHORT usDataLen;
    UINT unMergeBytes = 0; 

__lblGetNextNode: 
    pubData = (UCHAR *)buf_list_get_next_node(&sNextNode, &usDataLen);
    if (NULL == pubData)
        return; 

    //* 搬运数据
    memcpy(pubPacket + unMergeBytes, pubData, usDataLen);
    unMergeBytes += (UINT)usDataLen; 

    goto __lblGetNextNode;
}

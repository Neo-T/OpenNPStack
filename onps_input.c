#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "utils.h"

#define SYMBOL_GLOBALS
#include "onps_input.h"
#undef SYMBOL_GLOBALS

//* 依然是静态分配，丰俭由SOCKET_NUM_MAX宏决定，动态分配对于资源受限的单片机系统来说不可控，尤其是对于堆和栈来说
static STCB_ONPS_INPUT l_stcbaInput[SOCKET_NUM_MAX]; 
static ST_SLINKEDLIST_NODE l_staNodes[SOCKET_NUM_MAX];
static HMUTEX l_hMtxInput = INVALID_HMUTEX; 
static PST_SLINKEDLIST l_pstFreedSLList = NULL; 
static PST_SLINKEDLIST l_pstInputSLList = NULL;
static USHORT l_usIcmpIdentifier = 0; 

BOOL onps_input_init(EN_ERROR_CODE *penErrCode)
{
    *penErrCode = ERRNO;

    l_hMtxInput = os_thread_mutex_init();
    if (INVALID_HMUTEX == l_hMtxInput)
    {
        *penErrCode = ERRMUTEXINITFAILED;
        return FALSE; 
    }

    //* 全部清零
    memset(&l_stcbaInput[0], 0, sizeof(l_stcbaInput));
    memset(&l_staNodes[0], 0, sizeof(l_staNodes));

    //* 生成单向链表
    INT i;     
    l_staNodes[0].pvData = &l_stcbaInput[0];
    l_stcbaInput[0].hSem = INVALID_HSEM; 
    for (i = 1; i < SOCKET_NUM_MAX; i++)
    {
        l_staNodes[i - 1].pstNext = &l_staNodes[i];        
        l_stcbaInput[i].hSem = INVALID_HSEM;
        l_staNodes[i].pvData = &l_stcbaInput[i];         
    }

    l_pstFreedSLList = &l_staNodes[0];
}

void onps_input_uninit(void)
{
    INT i;
    for (i = 0; i < SOCKET_NUM_MAX; i++)
    {
        if (l_stcbaInput[i].pubRcvBuf)
        {
            buddy_free(l_stcbaInput[i].pubRcvBuf);
            l_stcbaInput[i].pubRcvBuf = NULL;
        }

        if (INVALID_HSEM != l_stcbaInput[i].hSem)
        {
            os_thread_sem_uninit(l_stcbaInput[i].hSem);
            l_stcbaInput[i].hSem = INVALID_HSEM;
        }
    }

    if (INVALID_HMUTEX != l_hMtxInput)
    {
        os_thread_mutex_uninit(l_hMtxInput);
        l_hMtxInput = INVALID_HMUTEX; 
    }
}

PSTCB_ONPS_INPUT onps_input_new(EN_IPPROTO enProtocol, EN_ERROR_CODE *penErrCode)
{
    HSEM hSem = os_thread_sem_init(0, 1);
    if (INVALID_HSEM == hSem)
    {
        if (penErrCode)
            *penErrCode = ERRSEMINITFAILED;
        return NULL; 
    }

    UINT unSize; 
    switch (enProtocol)
    {
    case IPPROTO_ICMP:
        unSize = ICMPRCVBUF_SIZE_DEFAULT;
        break; 

    case IPPROTO_TCP:
        unSize = TCPRCVBUF_SIZE_DEFAULT;
        break; 

    case IPPROTO_UDP:
        unSize = UDPRCVBUF_SIZE_DEFAULT;
        break; 

    default:
        if (penErrCode)
            *penErrCode = ERRUNSUPPIPPROTO;
        return NULL; 
    }

    UCHAR *pubRcvBuf = (UCHAR *)buddy_alloc(unSize, penErrCode);
    if (NULL == pubRcvBuf)
    {
        os_thread_sem_uninit(hSem);  
        return NULL;
    }

    //* 申请一个input节点
    PST_SLINKEDLIST_NODE pstNode; 
    PSTCB_ONPS_INPUT pstcbInput; 
    os_thread_mutex_lock(l_hMtxInput); 
    {
        pstNode = sllist_get_node(&l_pstFreedSLList);
        pstcbInput = (PSTCB_ONPS_INPUT)pstNode->pvData; 
        pstcbInput->hSem = hSem; 
        pstcbInput->ubIPProto = (UCHAR)enProtocol; 
        pstcbInput->pubRcvBuf = pubRcvBuf; 
        pstcbInput->unRcvBufSize = unSize;

        if (IPPROTO_ICMP == enProtocol)
            pstcbInput->uniHandle.stIcmp.usIdentifier = l_usIcmpIdentifier++;
        else if (IPPROTO_TCP == enProtocol)
        {
            pstcbInput->uniHandle.stTcp.unIP = 0;
            pstcbInput->uniHandle.stTcp.usPort = 0; 
        }
        else if (IPPROTO_UDP == enProtocol)
        {
            pstcbInput->uniHandle.stUdp.unIP = 0;
            pstcbInput->uniHandle.stUdp.usPort = 0;
        }
        else;         

        sllist_put_node(&l_pstInputSLList, pstNode); 
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return pstcbInput; 
}

void onps_input_free(PSTCB_ONPS_INPUT pstInput)
{
    //* 先释放申请的相关资源
    if (pstInput->pubRcvBuf)
    {
        buddy_free(pstInput->pubRcvBuf);
        pstInput->pubRcvBuf = NULL; 
    }    
    if (INVALID_HSEM != pstInput->hSem)
    {
        os_thread_sem_uninit(pstInput->hSem);
        pstInput->hSem = INVALID_HSEM;
    }

    //* 归还节点
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList; 
        while (pstNextNode)
        {
            if ((PSTCB_ONPS_INPUT)pstNextNode->pvData == pstInput)
            {
                sllist_del_node(&l_pstInputSLList, pstNextNode); //* 从input链表摘除
                sllist_put_node(&l_pstFreedSLList, pstNextNode); //* 归还给free链表
                break; 
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);
}

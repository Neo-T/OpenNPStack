#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "onps_utils.h"
#include "one_shot_timer.h"
#define SYMBOL_GLOBALS
#include "ip/tcp_link.h"
#undef SYMBOL_GLOBALS

//* tcp链路
static ST_TCPLINK l_staTcpLinkNode[TCP_LINK_NUM_MAX]; 
static PST_TCPLINK l_pstFreeTcpLinkList = NULL; 
static HMUTEX l_hMtxTcpLinkList = INVALID_HMUTEX; 

//* 与tcp服务器业务逻辑相关的静态存储时期的变量
static ST_INPUTATTACH_TCPSRV l_staTcpSrv[TCPSRV_NUM_MAX]; 
static ST_TCPBACKLOG_NODE l_staTcpBacklog[TCPSRV_BACKLOG_NUM_MAX]; 
static ST_SLINKEDLIST_NODE l_staTcpBacklogList[TCPSRV_BACKLOG_NUM_MAX]; 

BOOL tcp_link_init(EN_ONPSERR *penErr)
{
    //* 链接	
    INT i;
    for (i = 0; i < TCP_LINK_NUM_MAX - 1; i++)
    {
        l_staTcpLinkNode[i].bIdx = i; 
        l_staTcpLinkNode[i].bNext = i + 1;
    }
    l_staTcpLinkNode[i].bIdx = i; 
    l_staTcpLinkNode[i].bNext = -1; 
    l_pstFreeTcpLinkList = &l_staTcpLinkNode[0]; 

    l_hMtxTcpLinkList = os_thread_mutex_init();
    if (INVALID_HMUTEX != l_hMtxTcpLinkList)
        return TRUE;

    if (penErr)
        *penErr = ERRMUTEXINITFAILED;
    return FALSE;
}

void tcp_link_uninit(void)
{
    if (INVALID_HMUTEX != l_hMtxTcpLinkList)
        os_thread_mutex_uninit(l_hMtxTcpLinkList);
}

PST_TCPLINK tcp_link_get(EN_ONPSERR *penErr)
{
    PST_TCPLINK pstFreeNode;
    os_thread_mutex_lock(l_hMtxTcpLinkList);
    {
        if (NULL == l_pstFreeTcpLinkList)
        {
            os_thread_mutex_unlock(l_hMtxTcpLinkList);

            if (penErr)
                *penErr = ERRNOTCPLINKNODE;

            return NULL;
        }

        pstFreeNode = l_pstFreeTcpLinkList;
        if (l_pstFreeTcpLinkList->bNext >= 0)
            l_pstFreeTcpLinkList = &l_staTcpLinkNode[l_pstFreeTcpLinkList->bNext]; 
        else        
            l_pstFreeTcpLinkList = NULL;
    }
    os_thread_mutex_unlock(l_hMtxTcpLinkList);

    pstFreeNode->bState = TLSINIT;
    pstFreeNode->stLocal.unSeqNum = pstFreeNode->stLocal.unAckNum = pstFreeNode->stPeer.unSeqNum = 0; 
    pstFreeNode->stPeer.bSackEn = FALSE;
    pstFreeNode->stPeer.bWndScale = 0;
    pstFreeNode->stPeer.usMSS = 1200; 
    pstFreeNode->stPeer.usWndSize = 8192;     
    pstFreeNode->stLocal.bDataSendState = TDSSENDRDY;   //* 发送状态初始化
    return pstFreeNode;
}

void tcp_link_free(PST_TCPLINK pstTcpLink)
{    
    os_thread_mutex_lock(l_hMtxTcpLinkList);
    {
        if (l_pstFreeTcpLinkList)        
            pstTcpLink->bNext = l_pstFreeTcpLinkList->bIdx;         
        else        
            pstTcpLink->bNext = -1; 
        l_pstFreeTcpLinkList = pstTcpLink;
    }
    os_thread_mutex_unlock(l_hMtxTcpLinkList);
}

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
//* =========================================================================
static ST_INPUTATTACH_TCPSRV l_staIAttachSrv[TCPSRV_NUM_MAX]; 

//* 连接请求队列
static ST_TCPBACKLOG l_staBacklog[TCPSRV_BACKLOG_NUM_MAX]; 
static ST_SLINKEDLIST_NODE l_staSListBacklog[TCPSRV_BACKLOG_NUM_MAX]; 
static PST_SLINKEDLIST l_pstSListBacklogFreed; 

//* 数据接收队列
static ST_SLINKEDLIST_NODE l_staSListRcvQueue[TCPSRV_RECV_QUEUE_NUM]; 
static PST_SLINKEDLIST l_pstSListRcvQueueFreed; 
//* =========================================================================

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

    //* 组成backlog资源链（用于tcp服务器）    
    for (i = 0; i < TCPSRV_BACKLOG_NUM_MAX - 1; i++)
    {
        l_staSListBacklog[i].pstNext = &l_staSListBacklog[i + 1]; 
        l_staSListBacklog[i].uniData.ptr = &l_staBacklog[i]; 
    }
    l_staSListBacklog[i].pstNext = NULL;
    l_staSListBacklog[i].uniData.ptr = &l_staBacklog[i]; 
    l_pstSListBacklogFreed = &l_staSListBacklog[0]; 

    //* 清零tcp服务器的附加数据段资源
    memset(&l_staIAttachSrv[0], 0, sizeof(l_staIAttachSrv)); 

    //* 组成数据接收队列链资源（用于tcp服务器）
    for (i = 0; i < TCPSRV_RECV_QUEUE_NUM - 1; i++)
        l_staSListRcvQueue[i].pstNext = &l_staSListRcvQueue[i + 1]; 
    l_staSListRcvQueue[i].pstNext = NULL; 
    l_pstSListRcvQueueFreed = &l_staSListRcvQueue[0]; 

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


PST_INPUTATTACH_TCPSRV tcpsrv_input_attach_get(EN_ONPSERR *penErr)
{
    PST_INPUTATTACH_TCPSRV pstAttach = NULL;     
    
    os_critical_init();
    os_enter_critical();
    {
        INT i;
        for (i = 0; i < TCPSRV_NUM_MAX; i++)
        {
            if (!l_staIAttachSrv[i].bIsUsed)
            {
                pstAttach = &l_staIAttachSrv[i]; 
                pstAttach->bIsUsed = TRUE; 
                break; 
            }
        }
    }
    os_exit_critical(); 

    if (pstAttach)
    {        
        pstAttach->pstSListBacklog = NULL; 

        pstAttach->hSemAccept = os_thread_sem_init(0, TCPSRV_BACKLOG_NUM_MAX); 
        if (INVALID_HSEM == pstAttach->hSemAccept) 
        {
            if (penErr)
                *penErr = ERRSEMINITFAILED;
            pstAttach->bIsUsed = FALSE; 
            pstAttach = NULL; 
        }
    }
    else
    {
        if (penErr)
            *penErr = ERRTCPSRVEMPTY;
    }

    return pstAttach; 
}

void tcpsrv_input_attach_free(PST_INPUTATTACH_TCPSRV pstAttach)
{
    os_thread_sem_uninit(pstAttach->hSemAccept); 
    pstAttach->bIsUsed = FALSE; 
}

PST_TCPBACKLOG tcp_backlog_freed_get(EN_ONPSERR *penErr)
{
    PST_TCPBACKLOG pstBacklog = NULL;

    os_critical_init();
    os_enter_critical();
    {
        PST_SLINKEDLIST_NODE pstNode = sllist_get_node(&l_pstSListBacklogFreed);
        if (pstNode)
        {
            pstBacklog = (PST_TCPBACKLOG)pstNode->uniData.ptr;
            pstBacklog->pstNode = pstNode;             
        }
    }
    os_exit_critical();

    if (!pstBacklog)
    {
        if (penErr)
            *penErr = ERRTCPBACKLOGEMPTY; 
    }

    return pstBacklog; 
}

PST_TCPBACKLOG tcp_backlog_get(PST_SLINKEDLIST *ppstSListBacklog, USHORT *pusBacklogCnt)
{
    PST_TCPBACKLOG pstBacklog = NULL; 

    os_critical_init(); 
    os_enter_critical();
    {
        PST_SLINKEDLIST_NODE pstNode = sllist_get_node(ppstSListBacklog); 
        if (pstNode)
        {
            pstBacklog = (PST_TCPBACKLOG)pstNode->uniData.ptr;
            (*pusBacklogCnt)--; 
        }
    }
    os_exit_critical(); 

    return pstBacklog; 
}

void tcp_backlog_put(PST_SLINKEDLIST *ppstSListBacklog, PST_TCPBACKLOG pstBacklog, USHORT *pusBacklogCnt)
{
    os_critical_init();
    os_enter_critical(); 
    {
        (*pusBacklogCnt)++; 
        sllist_put_tail_node(ppstSListBacklog, pstBacklog->pstNode);
    }
    os_exit_critical();
}

void tcp_backlog_free(PST_TCPBACKLOG pstBacklog)
{
    os_critical_init();
    os_enter_critical();
    {
        sllist_put_node(&l_pstSListBacklogFreed, pstBacklog->pstNode);
    }
    os_exit_critical();
}

PST_TCPSRV_RCVQUEUE_NODE tcpsrv_recv_queue_freed_get(EN_ONPSERR *penErr)
{
    PST_TCPSRV_RCVQUEUE_NODE pstNode = NULL; 

    os_critical_init();
    os_enter_critical();
    {
        pstNode = sllist_get_node(&l_pstSListRcvQueueFreed);        
    }
    os_exit_critical(); 

    if (!pstNode)
    {
        if (penErr)
            *penErr = ERRTCPRCVQUEUEEMPTY;
    }

    return pstNode; 
}

PST_TCPSRV_RCVQUEUE_NODE tcpsrv_recv_queue_get(PST_SLINKEDLIST *ppstSListRcvQueue)
{
    PST_TCPSRV_RCVQUEUE_NODE pstNode = NULL;

    os_critical_init();
    os_enter_critical();
    {
        pstNode = sllist_get_node(ppstSListRcvQueue);
    }
    os_exit_critical();

    return pstNode; 
}

void tcpsrv_recv_queue_put(PST_SLINKEDLIST *ppstSListRcvQueue, PST_TCPSRV_RCVQUEUE_NODE pstNode, INT nInput)
{
    os_critical_init(); 
    os_enter_critical(); 
    {
        pstNode->uniData.nVal = nInput;
        sllist_put_tail_node(ppstSListRcvQueue, pstNode); 
    }
    os_exit_critical();
}

void tcpsrv_recv_queue_free(PST_TCPSRV_RCVQUEUE_NODE pstNode)
{
    os_critical_init();
    os_enter_critical();
    {
        sllist_put_node(&l_pstSListRcvQueueFreed, pstNode);
    }
    os_exit_critical();
}

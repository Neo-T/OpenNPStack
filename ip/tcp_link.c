/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "onps_utils.h"
#include "one_shot_timer.h"
#include "mmu/buddy.h"
#include "onps_input.h"
#define SYMBOL_GLOBALS
#include "ip/tcp_link.h"
#undef SYMBOL_GLOBALS

//* tcp链路
static ST_TCPLINK l_staTcpLinkNode[TCP_LINK_NUM_MAX]; 
static PST_TCPLINK l_pstFreeTcpLinkList = NULL; 
static HMUTEX l_hMtxTcpLinkList = INVALID_HMUTEX; 
static PST_TCPLINK l_pstUsedTcpLinkList = NULL; 

#if SUPPORT_ETHERNET && (TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV)
//* 与tcp服务器业务逻辑相关的静态存储时期的变量
//* =========================================================================
#ifdef TELNETSRV_SUPPORT_IPv6
static ST_INPUTATTACH_TCPSRV l_staIAttachSrv[TCPSRV_NUM_MAX + NETTOOLS_TELNETSRV + TELNETSRV_SUPPORT_IPv6];
#else
static ST_INPUTATTACH_TCPSRV l_staIAttachSrv[TCPSRV_NUM_MAX + NETTOOLS_TELNETSRV];
#endif //* #ifdef TELNETSRV_SUPPORT_IPv6

//* 连接请求队列
static ST_TCPBACKLOG l_staBacklog[TCPSRV_BACKLOG_NUM_MAX]; 
static ST_SLINKEDLIST_NODE l_staSListBacklog[TCPSRV_BACKLOG_NUM_MAX]; 
static PST_SLINKEDLIST l_pstSListBacklogFreed; 

//* 数据接收队列
static ST_SLINKEDLIST_NODE l_staSListRcvQueue[TCPSRV_RECV_QUEUE_NUM]; 
static PST_SLINKEDLIST l_pstSListRcvQueueFreed;
//* =========================================================================
#endif //* #if SUPPORT_ETHERNET && (TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV)

#if SUPPORT_SACK
static STCB_TCPSENDTIMER l_stcbaSndTimer[TCP_LINK_NUM_MAX * TCPSENDTIMER_NUM]; //* 每一路tcp链路允许开启TCPSENDTIMER_NUM个定时器
static PSTCB_TCPSENDTIMER l_pstcbSListSndTimerFreed = NULL;
static PSTCB_TCPSENDTIMER l_pstcbSListSndTimer = NULL; 
static PSTCB_TCPSENDTIMER l_pstcbSListSndTimerTail = NULL;
static HMUTEX l_hMtxSndTimerLink = INVALID_HMUTEX;
static HSEM l_hSemSndForSack = INVALID_HSEM; 
static HMUTEX l_hMtxSndDataLink = INVALID_HMUTEX; 
static PST_TCPLINK l_pstSndDataLink = NULL;
#endif

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

#if SUPPORT_ETHERNET && (TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV)
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
#endif //* #if SUPPORT_ETHERNET && (TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV)

#if SUPPORT_SACK
    for (i = 0; i < sizeof(l_stcbaSndTimer) / sizeof(STCB_TCPSENDTIMER) - 1; i++)    
        l_stcbaSndTimer[i].pstcbNext = &l_stcbaSndTimer[i + 1]; 
    l_stcbaSndTimer[i].pstcbNext = NULL; 
    l_pstcbSListSndTimerFreed = &l_stcbaSndTimer[0];

    //* 用于发送定时器队列的互斥锁
    l_hMtxSndTimerLink = os_thread_mutex_init(); 
    if (INVALID_HMUTEX == l_hMtxSndTimerLink)
    {
        if (penErr)
            *penErr = ERRMUTEXINITFAILED;
        return FALSE;
    }

    //* 用于数据发送队列的互斥锁
    l_hMtxSndDataLink = os_thread_mutex_init(); 
    if (INVALID_HMUTEX == l_hMtxSndDataLink)
    {
        os_thread_mutex_uninit(l_hMtxSndTimerLink);

        if (penErr)
            *penErr = ERRMUTEXINITFAILED;
        return FALSE;
    }

    //* 用于数据发送队列的信号量
    l_hSemSndForSack = os_thread_sem_init(0, 1000000000);
    if (INVALID_HSEM == l_hSemSndForSack)
    {
        os_thread_mutex_uninit(l_hMtxSndTimerLink);
        os_thread_mutex_uninit(l_hMtxSndDataLink); 

        if (penErr)
            *penErr = ERRSEMINITFAILED;
        return FALSE; 
    }
#endif

    l_hMtxTcpLinkList = os_thread_mutex_init();
    if (INVALID_HMUTEX != l_hMtxTcpLinkList)
        return TRUE;

#if SUPPORT_SACK
    os_thread_sem_uninit(l_hSemSndForSack);
    l_hSemSndForSack = INVALID_HSEM; 
#endif

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

        if (l_pstUsedTcpLinkList)
            pstFreeNode->bNext = l_pstUsedTcpLinkList->bIdx;
        else
            pstFreeNode->bNext = -1;
        l_pstUsedTcpLinkList = pstFreeNode;
    }
    os_thread_mutex_unlock(l_hMtxTcpLinkList);

#if SUPPORT_SACK
    pstFreeNode->stcbSend.pubSndBuf = (UCHAR *)buddy_alloc(TCPSNDBUF_SIZE, penErr); 
    if (!pstFreeNode->stcbSend.pubSndBuf)
    {
        tcp_link_free(pstFreeNode); 
        return NULL; 
    }    
    pstFreeNode->stcbSend.unWriteBytes = 0; 
    pstFreeNode->stcbSend.bNext = -1; 
    pstFreeNode->stcbSend.bSendPacketNum = 0; 
    pstFreeNode->stcbSend.bDupAckNum = 0; 
    pstFreeNode->stcbSend.unPrevSeqNum = 1; 
    //pstFreeNode->stcbSend.unRetransSeqNum = 0;     
    pstFreeNode->stcbSend.bIsPutted = FALSE;
    pstFreeNode->stcbSend.bIsWndSizeUpdated = TRUE; 
    pstFreeNode->stcbSend.bIsZeroWnd = FALSE; 
    pstFreeNode->stcbSend.unLastSndZeroWndPktMSecs = 0; 
    memset(&pstFreeNode->stcbSend.staSack, 0, sizeof(pstFreeNode->stcbSend.staSack));    
#endif

    pstFreeNode->bState = TLSINIT;
    pstFreeNode->stLocal.unSeqNum = pstFreeNode->stPeer.unSeqNum = 0;
    pstFreeNode->uniFlags.usVal = 0; 
    pstFreeNode->stPeer.bSackEn = FALSE;
    pstFreeNode->stPeer.bWndScale = 0;
    pstFreeNode->stPeer.usMSS = 1200; 
    pstFreeNode->stPeer.usWndSize = 8192; 
#if SUPPORT_SACK
    pstFreeNode->stcbSend.unWndSize = pstFreeNode->stPeer.usWndSize;
    pstFreeNode->stLocal.unAckedSeqNum = 0; 
	pstFreeNode->stLocal.unHasSndBytes = 0; 
#endif
    pstFreeNode->stPeer.bIsNotAcked = FALSE;
    pstFreeNode->stLocal.bDataSendState = TDSSENDRDY;   //* 发送状态初始化
    return pstFreeNode;
}

void tcp_link_free(PST_TCPLINK pstTcpLink)
{    
#if SUPPORT_SACK
    if (pstTcpLink->stcbSend.pubSndBuf)            
        buddy_free(pstTcpLink->stcbSend.pubSndBuf);            

    //* 释放占用的send timer
    tcp_send_timer_lock();
    {
        PSTCB_TCPSENDTIMER pstNextSendTimer = pstTcpLink->stcbSend.pstcbSndTimer;
        PSTCB_TCPSENDTIMER pstSendTimer; 
        while (pstNextSendTimer)
        {                
            pstSendTimer = pstNextSendTimer; 
            pstNextSendTimer = pstNextSendTimer->pstcbNextForLink;  //* 继续取出下一个节点
            tcp_send_timer_node_del_unsafe(pstSendTimer);           //* 从定时器队列中删除当前节点                
            tcp_send_timer_node_free_unsafe(pstSendTimer);          //* 归还当前节点                                                                                               
        }
    }
    pstTcpLink->stcbSend.pstcbSndTimer = NULL; 
    tcp_send_timer_unlock();   

    //* 数据发送队列删除
    tcp_link_for_send_data_del(pstTcpLink);
#endif

    os_thread_mutex_lock(l_hMtxTcpLinkList);
    {                
        //* 先从使用队列中摘除
        PST_TCPLINK pstNextNode = l_pstUsedTcpLinkList;
        PST_TCPLINK pstPrevNode = NULL; 
        while (pstNextNode)
        {
            if (pstTcpLink == pstNextNode)
            {
                if (pstPrevNode)
                    pstPrevNode->bNext = pstTcpLink->bNext;
                else
                {
                    if (pstTcpLink->bNext >= 0)
                        l_pstUsedTcpLinkList = &l_staTcpLinkNode[pstTcpLink->bNext];
                    else //* 这即是第一个节点也是最后一个节点
                        l_pstUsedTcpLinkList = NULL; 
                }

                break; 
            }

            pstPrevNode = pstNextNode;
            if (pstNextNode->bNext < 0) //* 理论上不会出现小于0的情况在这之前应该能找到
                break; 
            pstNextNode = &l_staTcpLinkNode[pstNextNode->bNext];
        }

        if (l_pstFreeTcpLinkList)        
            pstTcpLink->bNext = l_pstFreeTcpLinkList->bIdx;         
        else        
            pstTcpLink->bNext = -1; 
        l_pstFreeTcpLinkList = pstTcpLink;
    }
    os_thread_mutex_unlock(l_hMtxTcpLinkList);
}

void tcp_link_list_used_put(PST_TCPLINK pstTcpLink)
{
    os_thread_mutex_lock(l_hMtxTcpLinkList);
    {
        if (l_pstUsedTcpLinkList)
            pstTcpLink->bNext = l_pstUsedTcpLinkList->bIdx;
        else
            pstTcpLink->bNext = -1; 
        l_pstUsedTcpLinkList = pstTcpLink; 
    }
    os_thread_mutex_unlock(l_hMtxTcpLinkList);
}

PST_TCPLINK tcp_link_list_used_get_next(PST_TCPLINK pstTcpLink)
{
    PST_TCPLINK pstNextNode = NULL; 

    if (pstTcpLink)
    {
        if (pstTcpLink->bNext >= 0)
            pstNextNode = &l_staTcpLinkNode[pstTcpLink->bNext];
    }
    else
    {
        pstNextNode = l_pstUsedTcpLinkList;
    }

    return pstNextNode; 
}

void tcp_link_lock(void)
{
    os_thread_mutex_lock(l_hMtxTcpLinkList);
}

void tcp_link_unlock(void)
{
    os_thread_mutex_unlock(l_hMtxTcpLinkList);
}

#if SUPPORT_SACK
void tcp_send_sem_post(void)
{
    os_thread_sem_post(l_hSemSndForSack);
}

INT tcp_send_sem_pend(INT nWaitSecs)
{
    return os_thread_sem_pend(l_hSemSndForSack, nWaitSecs);
}

PSTCB_TCPSENDTIMER tcp_send_timer_node_get(void)
{
    PSTCB_TCPSENDTIMER pstcbNode = NULL; 
    os_thread_mutex_lock(l_hMtxSndTimerLink);
    {
        if (l_pstcbSListSndTimerFreed)
        {
            pstcbNode = l_pstcbSListSndTimerFreed;
            l_pstcbSListSndTimerFreed = l_pstcbSListSndTimerFreed->pstcbNext;
            pstcbNode->pstcbNext = NULL; 
            pstcbNode->pstcbNextForLink = NULL; 
            pstcbNode->bIsNotSacked = TRUE; 
        }
    }
    os_thread_mutex_unlock(l_hMtxSndTimerLink);

    return pstcbNode;
}

void tcp_send_timer_node_free(PSTCB_TCPSENDTIMER pstcbSendTimer)
{
    os_thread_mutex_lock(l_hMtxSndTimerLink);
    {
        pstcbSendTimer->pstcbNext = l_pstcbSListSndTimerFreed;
        l_pstcbSListSndTimerFreed = pstcbSendTimer;
    }
    os_thread_mutex_unlock(l_hMtxSndTimerLink);
}

void tcp_send_timer_node_free_unsafe(PSTCB_TCPSENDTIMER pstcbSendTimer)
{
    pstcbSendTimer->pstcbNext = l_pstcbSListSndTimerFreed; 
    l_pstcbSListSndTimerFreed = pstcbSendTimer; 
}

void tcp_send_timer_node_put(PSTCB_TCPSENDTIMER pstcbSendTimer)
{
    pstcbSendTimer->pstcbNext = NULL;
    os_thread_mutex_lock(l_hMtxSndTimerLink); 
    {
        if (l_pstcbSListSndTimer)
        {
            l_pstcbSListSndTimerTail->pstcbNext = pstcbSendTimer;            
            l_pstcbSListSndTimerTail = pstcbSendTimer; 
        }
        else
        {
            l_pstcbSListSndTimer = pstcbSendTimer;             
            l_pstcbSListSndTimerTail = l_pstcbSListSndTimer;
        }
    }
    os_thread_mutex_unlock(l_hMtxSndTimerLink); 
}

void tcp_send_timer_node_put_unsafe(PSTCB_TCPSENDTIMER pstcbSendTimer)
{
    pstcbSendTimer->pstcbNext = NULL;

    if (l_pstcbSListSndTimer)
    {
        l_pstcbSListSndTimerTail->pstcbNext = pstcbSendTimer;
        l_pstcbSListSndTimerTail = pstcbSendTimer;
    }
    else
    {
        l_pstcbSListSndTimer = pstcbSendTimer;
        l_pstcbSListSndTimerTail = l_pstcbSListSndTimer;
    }
}

void tcp_send_timer_node_del(PSTCB_TCPSENDTIMER pstcbSendTimer)
{
    os_thread_mutex_lock(l_hMtxSndTimerLink); 
    {
        PSTCB_TCPSENDTIMER pstSendTimerNext = l_pstcbSListSndTimer;
        PSTCB_TCPSENDTIMER pstSendTimerPrev = NULL;
        while (pstSendTimerNext)
        {
            if (pstSendTimerNext == pstcbSendTimer)
            {                
                if (pstSendTimerPrev)
                    pstSendTimerPrev->pstcbNext = pstcbSendTimer->pstcbNext;
                else                
                    l_pstcbSListSndTimer = pstcbSendTimer->pstcbNext;

                //* 看看是否是尾部节点，如果是则更新尾部节点指针到前一个
                if (pstSendTimerNext == l_pstcbSListSndTimerTail)                
                    l_pstcbSListSndTimerTail = pstSendTimerPrev;                

                break;
            }

            pstSendTimerPrev = pstSendTimerNext;            
            pstSendTimerNext = pstSendTimerNext->pstcbNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxSndTimerLink); 
}

void tcp_send_timer_node_del_unsafe(PSTCB_TCPSENDTIMER pstcbSendTimer)
{
    PSTCB_TCPSENDTIMER pstSendTimerNext = l_pstcbSListSndTimer;
    PSTCB_TCPSENDTIMER pstSendTimerPrev = NULL;
    while (pstSendTimerNext)
    {
        if (pstSendTimerNext == pstcbSendTimer)
        {
            if (pstSendTimerPrev)
                pstSendTimerPrev->pstcbNext = pstcbSendTimer->pstcbNext;
            else
                l_pstcbSListSndTimer = pstcbSendTimer->pstcbNext;

            //* 看看是否是尾部节点，如果是则更新尾部节点指针到前一个
            if (pstSendTimerNext == l_pstcbSListSndTimerTail)
                l_pstcbSListSndTimerTail = pstSendTimerPrev;

            break;
        }

        pstSendTimerPrev = pstSendTimerNext;
        pstSendTimerNext = pstSendTimerNext->pstcbNext;
    }
}

void tcp_send_timer_lock(void)
{
    os_thread_mutex_lock(l_hMtxSndTimerLink);
}

void tcp_send_timer_unlock(void)
{
    os_thread_mutex_unlock(l_hMtxSndTimerLink); 
}

PSTCB_TCPSENDTIMER tcp_send_timer_get_next(PSTCB_TCPSENDTIMER pstcbSendTimer)
{
    if (pstcbSendTimer)    
        return pstcbSendTimer->pstcbNext;    
    else
        return l_pstcbSListSndTimer; 
}

void tcp_link_for_send_data_put(PST_TCPLINK pstTcpLink)
{
    PST_TCPLINK pstNext;     

    os_thread_mutex_lock(l_hMtxSndDataLink); 
    {
        if (pstTcpLink->stcbSend.bIsPutted)
        {
            os_thread_mutex_unlock(l_hMtxSndDataLink); 
            return;
        }

        pstTcpLink->stcbSend.bNext = -1;
        pstTcpLink->stcbSend.bIsPutted = TRUE;
        if (l_pstSndDataLink)
        {
            pstNext = l_pstSndDataLink; 
            do {
                if (pstNext->stcbSend.bNext < 0) //* 挂载到链表的尾部
                {
                    pstNext->stcbSend.bNext = pstTcpLink->bIdx;                     
                    break; 
                }
                else
                    pstNext = &l_staTcpLinkNode[pstNext->stcbSend.bNext];
            } while (TRUE);
        }
        else        
            l_pstSndDataLink = pstTcpLink;        
    }
    os_thread_mutex_unlock(l_hMtxSndDataLink);
}

void tcp_link_for_send_data_del(PST_TCPLINK pstTcpLink)
{
    os_thread_mutex_lock(l_hMtxSndDataLink);
    {
        if (pstTcpLink->stcbSend.bIsPutted)
        {
            PST_TCPLINK pstNext = l_pstSndDataLink;
            PST_TCPLINK pstPrev = NULL; 
            while (pstNext)
            {
                if (pstNext == pstTcpLink)
                {
                    if (pstPrev)
                        pstPrev->stcbSend.bNext = pstNext->stcbSend.bNext; 
                    else //* 链表第一个节点就匹配了
                    {
                        if (pstNext->stcbSend.bNext >= 0)
                            l_pstSndDataLink = &l_staTcpLinkNode[pstNext->stcbSend.bNext];
                        else
                            l_pstSndDataLink = NULL; 
                    }

                    break; 
                }

                pstPrev = pstNext;
                if (pstNext->stcbSend.bNext < 0) //* 理论上不会出现小于0的情况在这之前应该能找到，这是链表的最后一个节点
                    break;
                pstNext = &l_staTcpLinkNode[pstNext->stcbSend.bNext]; 
            }

            pstTcpLink->stcbSend.bIsPutted = FALSE; 
        }
    }
    os_thread_mutex_unlock(l_hMtxSndDataLink);    
}

PST_TCPLINK tcp_link_for_send_data_get_next(PST_TCPLINK pstTcpLink)
{
    PST_TCPLINK pstNext = NULL;

    os_thread_mutex_lock(l_hMtxSndDataLink);
    {
        if (pstTcpLink)
        {
            if (pstTcpLink->stcbSend.bNext >= 0)
                pstNext = &l_staTcpLinkNode[pstTcpLink->stcbSend.bNext];
        }
        else        
            pstNext = l_pstSndDataLink;        
    }
    os_thread_mutex_unlock(l_hMtxSndDataLink);

    return pstNext; 
}
#endif

#if SUPPORT_ETHERNET
PST_INPUTATTACH_TCPSRV tcpsrv_input_attach_get(EN_ONPSERR *penErr)
{
    PST_INPUTATTACH_TCPSRV pstAttach = NULL;     
#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV    
    os_critical_init();
    os_enter_critical();
    {
        INT i;
    #ifdef TELNETSRV_SUPPORT_IPv6
        for (i = 0; i < TCPSRV_NUM_MAX + NETTOOLS_TELNETSRV + TELNETSRV_SUPPORT_IPv6; i++)
    #else
        for (i = 0; i < TCPSRV_NUM_MAX + NETTOOLS_TELNETSRV; i++)
    #endif //* #ifdef TELNETSRV_SUPPORT_IPv6
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
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV

    return pstAttach; 
}

void tcpsrv_input_attach_free(PST_INPUTATTACH_TCPSRV pstAttach)
{
#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
    os_thread_sem_uninit(pstAttach->hSemAccept); 
    pstAttach->bIsUsed = FALSE; 
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
}

PST_TCPBACKLOG tcp_backlog_freed_get(EN_ONPSERR *penErr)
{
    PST_TCPBACKLOG pstBacklog = NULL;

#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
    PST_SLINKEDLIST_NODE pstNode; 

    os_critical_init();
    os_enter_critical();
    {
        /*PST_SLINKEDLIST_NODE*/ pstNode = sllist_get_node(&l_pstSListBacklogFreed);
        /*
        if (pstNode)
        {
            pstBacklog = (PST_TCPBACKLOG)pstNode->uniData.ptr;
            pstBacklog->pstNode = pstNode;             
        }
        */
    }
    os_exit_critical();

    if (pstNode)
    {
        pstBacklog = (PST_TCPBACKLOG)pstNode->uniData.ptr;
        pstBacklog->pstNode = pstNode;
    }

    if (!pstBacklog)
    {
        if (penErr)
            *penErr = ERRTCPBACKLOGEMPTY; 
    }
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV

    return pstBacklog; 
}

PST_TCPBACKLOG tcp_backlog_get(PST_SLINKEDLIST *ppstSListBacklog, USHORT *pusBacklogCnt)
{
    PST_TCPBACKLOG pstBacklog = NULL; 

#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
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
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV

    return pstBacklog; 
}

void tcp_backlog_put(PST_SLINKEDLIST *ppstSListBacklog, PST_TCPBACKLOG pstBacklog, USHORT *pusBacklogCnt)
{
#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
    os_critical_init();
    os_enter_critical(); 
    {
        (*pusBacklogCnt)++; 
        sllist_put_tail_node(ppstSListBacklog, pstBacklog->pstNode);
    }
    os_exit_critical();
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
}

void tcp_backlog_free(PST_TCPBACKLOG pstBacklog)
{
#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
    os_critical_init();
    os_enter_critical();
    {
        sllist_put_node(&l_pstSListBacklogFreed, pstBacklog->pstNode);
    }
    os_exit_critical(); 
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
}

PST_TCPSRV_RCVQUEUE_NODE tcpsrv_recv_queue_freed_get(EN_ONPSERR *penErr)
{
    PST_TCPSRV_RCVQUEUE_NODE pstNode = NULL; 

#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
    os_critical_init();
    os_enter_critical();
    {
        pstNode = sllist_get_node(&l_pstSListRcvQueueFreed);        
    }
    os_exit_critical(); 

#if 0
	os_thread_mutex_lock(o_hMtxPrintf);
	{
		printf("<G> %d\r\n", tcpsrv_recv_queue_count(&l_pstSListRcvQueueFreed));
	}
	os_thread_mutex_unlock(o_hMtxPrintf);
#endif

    if (!pstNode)
    {
        if (penErr)
            *penErr = ERRTCPRCVQUEUEEMPTY;
    }
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV

    return pstNode; 
}

PST_TCPSRV_RCVQUEUE_NODE tcpsrv_recv_queue_get(PST_SLINKEDLIST *ppstSListRcvQueue)
{
    PST_TCPSRV_RCVQUEUE_NODE pstNode = NULL;

#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
    os_critical_init();
    os_enter_critical();
    {
        pstNode = sllist_get_node(ppstSListRcvQueue);
    }
    os_exit_critical();

	//* 测试使用
#if 0
	os_thread_mutex_lock(o_hMtxPrintf);
	{
		printf("<Q> %d\r\n", tcpsrv_recv_queue_count(ppstSListRcvQueue));
	}
	os_thread_mutex_unlock(o_hMtxPrintf);
#endif
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV

    return pstNode; 
}

void tcpsrv_recv_queue_put(PST_SLINKEDLIST *ppstSListRcvQueue, PST_TCPSRV_RCVQUEUE_NODE pstNode, INT nInput)
{
#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
    os_critical_init(); 
    os_enter_critical(); 
    {
        pstNode->uniData.nVal = nInput;
        sllist_put_tail_node(ppstSListRcvQueue, pstNode); 
    }
    os_exit_critical();
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
}

void tcpsrv_recv_queue_free(PST_TCPSRV_RCVQUEUE_NODE pstNode)
{
#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
    os_critical_init();
    os_enter_critical();
    {
        sllist_put_node(&l_pstSListRcvQueueFreed, pstNode);		
    }
    os_exit_critical();


	//* 测试使用
#if 0
	os_thread_mutex_lock(o_hMtxPrintf);
	{
		printf("<F> %d\r\n", tcpsrv_recv_queue_count(&l_pstSListRcvQueueFreed)); 
	}
	os_thread_mutex_unlock(o_hMtxPrintf);
#endif
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
}

INT tcpsrv_recv_queue_count(PST_SLINKEDLIST *ppstSListRcvQueue)
{
	INT nCount = 0; 	

#if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV
	os_critical_init(); 
	os_enter_critical();
	{
		PST_SLINKEDLIST_NODE pstNextNode = (PST_SLINKEDLIST_NODE)(*ppstSListRcvQueue); 
		while (pstNextNode)
		{
			nCount++; 
			pstNextNode = pstNextNode->pstNext;
		}
	}
	os_exit_critical();
#endif //* #if TCPSRV_NUM_MAX || NETTOOLS_TELNETSRV

	return nCount; 
}
#endif

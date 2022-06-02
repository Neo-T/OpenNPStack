#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#define SYMBOL_GLOBALS
#include "onps_input.h"
#undef SYMBOL_GLOBALS
#include "ip/tcp_link.h"
#include "ip/udp_link.h"

//* 协议栈icmp/tcp/udp层接收处理节点
typedef struct _STCB_ONPS_INPUT_ {
    HSEM hSem;  //* 信号量，一旦收到数据，协议栈将投递信号量给上层接收者，避免上层调用者采用轮询读取数据的方式，避免CPU资源被过多占用    

    union {  //* 系统分配的接收者句柄，根据不同的上层协议其句柄信息均有所不同
        struct {
            USHORT usIdentifier;
        } stIcmp; //* icmp层句柄

        ST_TCPUDP_HANDLE stAddr; //* 句柄，使用IP地址和端口就可以唯一的标识一个tcp/udp通讯链路
    } uniHandle;

    UCHAR ubIPProto;    //* IP上层协议定义，目前支持icmp/tcp/udp接收，其值来自于ip_frame.h中EN_IPPROTO枚举定义
    UCHAR ubLastErr;    //* 最近的错误信息，实际类型为EN_ONPSERR
    CHAR bRcvTimeout;   //* 接收超时时间（单位：秒），大于0，指定等待的最长秒数；0，不等待，直接返回；-1，一直等待直至数据到达

    UCHAR *pubRcvBuf;
    UINT unRcvBufSize;
    UINT unRcvedBytes; 
    void *pvAttach; 
} STCB_ONPS_INPUT, *PSTCB_ONPS_INPUT;

//* 依然是静态分配，丰俭由SOCKET_NUM_MAX宏决定，动态分配对于资源受限的单片机系统来说不可控，尤其是对于堆和栈来说
static STCB_ONPS_INPUT l_stcbaInput[SOCKET_NUM_MAX]; 
static ST_SLINKEDLIST_NODE l_staNodes[SOCKET_NUM_MAX];
static HMUTEX l_hMtxInput = INVALID_HMUTEX; 
static PST_SLINKEDLIST l_pstFreedSLList = NULL; 
static PST_SLINKEDLIST l_pstInputSLList = NULL;

BOOL onps_input_init(EN_ONPSERR *penErr)
{
    *penErr = ERRNO;

    if (!tcp_link_init(penErr))
        return FALSE; 

    if (!udp_link_init(penErr))
    {
        udp_link_uninit();
        return FALSE;
    }

    l_hMtxInput = os_thread_mutex_init();
    if (INVALID_HMUTEX == l_hMtxInput)
    {
        udp_link_uninit();
        tcp_link_uninit();

        *penErr = ERRMUTEXINITFAILED;
        return FALSE; 
    }

    //* 全部清零
    memset(&l_stcbaInput[0], 0, sizeof(l_stcbaInput));
    memset(&l_staNodes[0], 0, sizeof(l_staNodes));

    //* 生成单向链表
    INT i;     
    l_staNodes[0].uniData.nIndex = 0;
    l_stcbaInput[0].hSem = INVALID_HSEM; 
    for (i = 1; i < SOCKET_NUM_MAX; i++)
    {
        l_staNodes[i - 1].pstNext = &l_staNodes[i];        
        l_stcbaInput[i].hSem = INVALID_HSEM;
        l_staNodes[i].uniData.nIndex = i; 
    }

    l_pstFreedSLList = &l_staNodes[0];

    return TRUE;
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

    udp_link_uninit();
    tcp_link_uninit(); 
}

INT onps_input_new(EN_IPPROTO enProtocol, EN_ONPSERR *penErr)
{
    HSEM hSem = os_thread_sem_init(0, 1);
    if (INVALID_HSEM == hSem)
    {
        if (penErr)
            *penErr = ERRSEMINITFAILED;
        return -1; 
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
        if (penErr)
            *penErr = ERRUNSUPPIPPROTO;
        return -1; 
    }

    UCHAR *pubRcvBuf = (UCHAR *)buddy_alloc(unSize, penErr);
    if (NULL == pubRcvBuf)
    {
        os_thread_sem_uninit(hSem);  
        return -1;
    }

    //* 申请一个input节点
    PST_SLINKEDLIST_NODE pstNode; 
    PSTCB_ONPS_INPUT pstcbInput; 
    os_thread_mutex_lock(l_hMtxInput); 
    {
        pstNode = sllist_get_node(&l_pstFreedSLList);
        pstcbInput = &l_stcbaInput[pstNode->uniData.nIndex];
        pstcbInput->hSem = hSem; 
        pstcbInput->bRcvTimeout = -1; 
        pstcbInput->ubIPProto = (UCHAR)enProtocol; 
        pstcbInput->pubRcvBuf = pubRcvBuf; 
        pstcbInput->unRcvBufSize = unSize;
        pstcbInput->unRcvedBytes = 0; 

        if (IPPROTO_ICMP == enProtocol)
            pstcbInput->uniHandle.stIcmp.usIdentifier = 0;        
        else
        {
            pstcbInput->uniHandle.stAddr.unNetifIp = 0;
            pstcbInput->uniHandle.stAddr.usPort = 0;            
            pstcbInput->pvAttach = NULL;
        }

        sllist_put_node(&l_pstInputSLList, pstNode); 
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return pstNode->uniData.nIndex;
}

void onps_input_free(INT nInput)
{
    if (nInput > SOCKET_NUM_MAX - 1)
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("onps_input_free() failed, Handle %d is out of system scope\r\n", nInput);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return; 
    }    

    //* 归还节点
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList; 
        while (pstNextNode)
        {
            if (pstNextNode->uniData.nIndex == nInput)
            {
                PSTCB_ONPS_INPUT pstcbInput = &l_stcbaInput[nInput];

                //* 释放tcp/udp协议附加的保存通讯链路相关信息的数据节点
                if (pstcbInput->pvAttach)
                {
                    if (IPPROTO_TCP == pstcbInput->ubIPProto)
                        tcp_link_free((PST_TCPLINK)pstcbInput->pvAttach);
                }

                //* 先释放申请的相关资源
                if (pstcbInput->pubRcvBuf)
                {
                    buddy_free(pstcbInput->pubRcvBuf);
                    pstcbInput->pubRcvBuf = NULL;
                }
                if (INVALID_HSEM != pstcbInput->hSem)
                {
                    os_thread_sem_uninit(pstcbInput->hSem);
                    pstcbInput->hSem = INVALID_HSEM;
                }

                sllist_del_node(&l_pstInputSLList, pstNextNode); //* 从input链表摘除
                sllist_put_node(&l_pstFreedSLList, pstNextNode); //* 归还给free链表
                break; 
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);
}

BOOL onps_input_set(INT nInput, ONPSIOPT enInputOpt, void *pvVal, EN_ONPSERR *penErr)
{
    if (nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;

        return FALSE;
    }

    PSTCB_ONPS_INPUT pstcbInput = &l_stcbaInput[nInput];
    switch (enInputOpt)
    {    
    case IOPT_SETICMPECHOID:
        if ((EN_IPPROTO)pstcbInput->ubIPProto == IPPROTO_ICMP)
            pstcbInput->uniHandle.stIcmp.usIdentifier = *((USHORT *)pvVal); 
        else
            goto __lblIpProtoNotMatched;
        break;

    case IOPT_SETTCPUDPADDR:
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto || IPPROTO_UDP == (EN_IPPROTO)pstcbInput->ubIPProto)
            pstcbInput->uniHandle.stAddr = *((PST_TCPUDP_HANDLE)pvVal);
        else
            goto __lblIpProtoNotMatched; 
        break; 

    case IOPT_SETTCPLINKSTATE: 
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto)
            ((PST_TCPLINK)(pstcbInput->pvAttach))->bState = *((CHAR *)pvVal);
        else
        {
            if (penErr)
                *penErr = ERRTCSNONTCP;
            return FALSE;
        }
        break; 

    case IOPT_SETATTACH:
        pstcbInput->pvAttach = pvVal; 
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto)
        {
            ((PST_TCPLINK)pstcbInput->pvAttach)->stLocal.usWndSize = pstcbInput->unRcvBufSize;
            ((PST_TCPLINK)pstcbInput->pvAttach)->stLocal.bIsZeroWnd = FALSE;
            ((PST_TCPLINK)pstcbInput->pvAttach)->stLocal.pstAddr = &pstcbInput->uniHandle.stAddr; 
            ((PST_TCPLINK)pstcbInput->pvAttach)->stcbWaitAck.bRcvTimeout = pstcbInput->bRcvTimeout; 
            ((PST_TCPLINK)pstcbInput->pvAttach)->stcbWaitAck.nInput = nInput; 
        }
        else if (IPPROTO_UDP == (EN_IPPROTO)pstcbInput->ubIPProto)
        {
            //* 暂时没有额外操作
        }
        else
            goto __lblIpProtoNotMatched;
        break; 

    case IOPT_SETRCVTIMEOUT:
        pstcbInput->bRcvTimeout = *((CHAR *)pvVal); 
        break; 

    default:
        if (penErr)
            *penErr = ERRUNSUPPIOPT; 
        return FALSE;
    }

    return TRUE; 

__lblIpProtoNotMatched: 
    if (penErr)
        *penErr = ERRIPROTOMATCH;
    return FALSE;
}

BOOL onps_input_get(INT nInput, ONPSIOPT enInputOpt, void *pvVal, EN_ONPSERR *penErr)
{
    if (nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;
        return FALSE;
    }

    PSTCB_ONPS_INPUT pstcbInput = &l_stcbaInput[nInput];
    switch (enInputOpt)
    {
    case IOPT_GETTCPUDPADDR: 
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto || IPPROTO_UDP == (EN_IPPROTO)pstcbInput->ubIPProto)
        {
            if (sizeof(pvVal) == 4)
                *((UINT *)pvVal) = (UINT)&pstcbInput->uniHandle.stAddr;
            else
                *((ULONGLONG *)pvVal) = (ULONGLONG)&pstcbInput->uniHandle.stAddr;
        }
        else
        {
            if (penErr)
                *penErr = ERRTCSNONTCP;
            return FALSE;
        }
        break; 

    case IOPT_GETSEM:
        *((HSEM *)pvVal) = pstcbInput->hSem; 
        break; 

    case IOPT_GETIPPROTO: 
        *((EN_IPPROTO *)pvVal) = (EN_IPPROTO)pstcbInput->ubIPProto; 
        break; 

    case IOPT_GETTCPLINKSTATE: 
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto)
        {
            if (pstcbInput->pvAttach)
                *((EN_TCPLINKSTATE *)pvVal) = (EN_TCPLINKSTATE)((PST_TCPLINK)(pstcbInput->pvAttach))->bState;
            else
                *((EN_TCPLINKSTATE *)pvVal) = TLSINVALID;
        }
        else
        {
            if (penErr)
                *penErr = ERRTCSNONTCP;
            return FALSE; 
        }
        break; 

    case IOPT_GETATTACH:
        if (sizeof(pvVal) == 4)
            *((UINT *)pvVal) = (UINT)pstcbInput->pvAttach;
        else
            *((ULONGLONG *)pvVal) = (ULONGLONG)pstcbInput->pvAttach;
        break; 

    case IOPT_GETTCPDATASNDSTATE:
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto)
        {
            if (pstcbInput->pvAttach)
                *((EN_TCPDATASNDSTATE *)pvVal) = (EN_TCPDATASNDSTATE)((PST_TCPLINK)(pstcbInput->pvAttach))->stLocal.bDataSendState;
            else
            {
                if (penErr)
                    *penErr = ERRNOATTACH;
                return FALSE;
            }
        }
        else
        {
            if (penErr)
                *penErr = ERRTDSNONTCP; 
            return FALSE;
        }
        break; 

    case IOPT_GETRCVTIMEOUT: 
        *((CHAR *)pvVal) = pstcbInput->bRcvTimeout; 
        break; 

    case IOPT_GETLASTSNDBYTES: 
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto)
        {
            if (pstcbInput->pvAttach)
                *((USHORT *)pvVal) = (USHORT)((PST_TCPLINK)(pstcbInput->pvAttach))->stcbWaitAck.usSendDataBytes; 
            else
            {
                if (penErr)
                    *penErr = ERRNOATTACH;
                return FALSE;
            }
        }
        else
        {
            if (penErr)
                *penErr = ERRTDSNONTCP;
            return FALSE;
        }
        break; 

    default:
        if (penErr)
            *penErr = ERRUNSUPPIOPT;
        return FALSE;
    }

    return TRUE;
}

void onps_input_post_sem(INT nInput)
{
    if (nInput > SOCKET_NUM_MAX - 1)
        return; 

    //* 只有bRecvTimeout不为0才需要等待报文到达信号，不为0意味着这是一个阻塞型input
    if (INVALID_HSEM != l_stcbaInput[nInput].hSem && l_stcbaInput[nInput].bRcvTimeout)
        os_thread_sem_post(l_stcbaInput[nInput].hSem); 
}

BOOL onps_input_set_tcp_close_state(INT nInput, CHAR bDstState)
{
    BOOL blRtnVal = FALSE; 
    if (IPPROTO_TCP == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)
    {
        PST_TCPLINK pstLink = (PST_TCPLINK)l_stcbaInput[nInput].pvAttach;
        os_thread_mutex_lock(l_hMtxInput);
        {            
            EN_TCPLINKSTATE enCurState = (EN_TCPLINKSTATE)pstLink->bState;
            if (TLSFINWAIT1 == (EN_TCPLINKSTATE)bDstState)
            {
                if (TLSCONNECTED == enCurState)
                {
                    pstLink->bState = (CHAR)TLSFINWAIT1;
                    pstLink->stcbWaitAck.bIsAcked = 0; 
                    blRtnVal = TRUE;
                }
                else if (TLSFINWAIT1 == enCurState) //* 说明在这之前至少本地或对端已经发送或收到一个FIN了，此时两端的FIN均已到达，但ACK尚未收到或发送，状态迁移到CLOSING态
                {
                    pstLink->bState = (CHAR)TLSCLOSING;
                    pstLink->stcbWaitAck.bIsAcked = 0;
                }
                else; //* 其它情况不再处理
            }
            else if (TLSFINWAIT2 == (EN_TCPLINKSTATE)bDstState)
            {
                if (TLSFINWAIT1 == enCurState)
                {
                    pstLink->bState = (CHAR)TLSFINWAIT2;
                    pstLink->stcbWaitAck.bIsAcked = 0; 
                    blRtnVal = TRUE;
                }                
                else; 
            }
            else if (TLSCLOSING == (EN_TCPLINKSTATE)bDstState)
            {
                if (TLSFINWAIT1 == enCurState)
                {
                    pstLink->bState = (CHAR)TLSCLOSING;
                    pstLink->stcbWaitAck.bIsAcked = 0;
                    blRtnVal = TRUE;
                }
                else; 
            }
            else if (TLSTIMEWAIT == (EN_TCPLINKSTATE)bDstState)
            {
                if (TLSFINWAIT2 == enCurState || TLSCLOSING == enCurState)
                {
                    pstLink->bState = (CHAR)TLSTIMEWAIT;
                    pstLink->stcbWaitAck.bIsAcked = 0;
                    blRtnVal = TRUE;
                }
                else; 
            }            
            else if (TLSCLOSED == (EN_TCPLINKSTATE)bDstState)
            {
                pstLink->bState = (CHAR)TLSCLOSED;
                pstLink->stcbWaitAck.bIsAcked = 0;
                blRtnVal = TRUE;
            }
            else; 
        }
        os_thread_mutex_unlock(l_hMtxInput);
    }    

    return blRtnVal; 
}

INT onps_input_tcp_close_time_count(INT nInput)
{
    INT nRtnVal = 0; 

    if (IPPROTO_TCP == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)
    {
        PST_TCPLINK pstLink = (PST_TCPLINK)l_stcbaInput[nInput].pvAttach; 
        os_thread_mutex_lock(l_hMtxInput);
        {
            pstLink->stcbWaitAck.bIsAcked++;
            if ((pstLink->stcbWaitAck.bIsAcked & 0x0F) >= 3) //* 根据目标资源情况调整此值，单位：秒，缩短或延长FIN操作时长以尽快或延缓资源释放时间
            {
                pstLink->stcbWaitAck.bIsAcked = (CHAR)(pstLink->stcbWaitAck.bIsAcked & 0xF0) + (CHAR)0x10;
                if (pstLink->stcbWaitAck.bIsAcked < 2 * 16) //* 小于3次超时则继续等待
                    nRtnVal = 1;
                else //* 已经连续6次超时即已经等待6 * 5 = 30秒了
                    nRtnVal = 2;                
            }
        }
        os_thread_mutex_unlock(l_hMtxInput);
    }

    return nRtnVal; 
}

void onps_input_lock(INT nInput)
{
    os_thread_mutex_lock(l_hMtxInput);
}

void onps_input_unlock(INT nInput)
{
    os_thread_mutex_unlock(l_hMtxInput);
}

INT onps_input_get_icmp(USHORT usIdentifier)
{
    INT nInput = -1; 
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList;
        PSTCB_ONPS_INPUT pstcbInput;
        while (pstNextNode)
        {
            pstcbInput = &l_stcbaInput[pstNextNode->uniData.nIndex];
            if ((EN_IPPROTO)pstcbInput->ubIPProto == IPPROTO_ICMP && usIdentifier == pstcbInput->uniHandle.stIcmp.usIdentifier)
            {
                nInput = pstNextNode->uniData.nIndex; 
                break;
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return nInput; 
}

BOOL onps_input_recv(INT nInput, const UCHAR *pubData, INT nDataBytes, EN_ONPSERR *penErr)
{
    if (nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;

        return FALSE; 
    }

    if (!pubData)
    {
        ((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->stLocal.usWndSize = l_stcbaInput[nInput].unRcvBufSize;
        return TRUE;
    }

    //* icmp报文只要是到达就直接覆盖前一组，无论前一组报文是否已被读取
    if (IPPROTO_ICMP == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)
    {        
        UINT unCpyBytes = (UINT)nDataBytes < l_stcbaInput[nInput].unRcvBufSize ? (UINT)nDataBytes : l_stcbaInput[nInput].unRcvBufSize; 
        memcpy(l_stcbaInput[nInput].pubRcvBuf, pubData, unCpyBytes);
        l_stcbaInput[nInput].unRcvedBytes = unCpyBytes;      

        //* 投递信号给上层用户，告知对端数据已到达
        if (l_stcbaInput[nInput].bRcvTimeout)
            os_thread_sem_post(l_stcbaInput[nInput].hSem);

        return TRUE; 
    }

    //* 将数据搬运到接收缓冲区
    os_thread_mutex_lock(l_hMtxInput);
    {
        UINT unCpyBytes = l_stcbaInput[nInput].unRcvBufSize - l_stcbaInput[nInput].unRcvedBytes; 
        unCpyBytes = unCpyBytes > (UINT)nDataBytes ? (UINT)nDataBytes : unCpyBytes; //* 由于存在流控（滑动窗口机制），理论上收到的数据应该一直小于等于缓冲区剩余容量才对，即unCpyBytes一直大于等于nDataBytes；
        memcpy(l_stcbaInput[nInput].pubRcvBuf + l_stcbaInput[nInput].unRcvedBytes, pubData, unCpyBytes); 
        l_stcbaInput[nInput].unRcvedBytes += unCpyBytes; 

        //* 如果当前input绑定的协议为tcp，则立即更新接收窗口大小
        if (IPPROTO_TCP == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)
        {
            ((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->stLocal.usWndSize = (USHORT)(l_stcbaInput[nInput].unRcvBufSize - l_stcbaInput[nInput].unRcvedBytes);
            if (!((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->stLocal.usWndSize)
                ((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->stLocal.bIsZeroWnd = TRUE;             
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);    

    //* 投递信号给上层用户，告知对端数据已到达
    if (l_stcbaInput[nInput].bRcvTimeout)
        os_thread_sem_post(l_stcbaInput[nInput].hSem); 

    return TRUE; 
}

INT onps_input_recv_upper(INT nInput, UCHAR *pubDataBuf, UINT unDataBufSize, EN_ONPSERR *penErr)
{
    if (nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;

        return -1;
    }

    if (TLSRESET == (EN_TCPLINKSTATE)((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->bState)
    {
        if (penErr)
            *penErr = ERRTCPCONNRESET; 
        return -1; 
    }
    else if (TLSTIMEWAIT == (EN_TCPLINKSTATE)((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->bState 
            || TLSCLOSING == (EN_TCPLINKSTATE)((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->bState)
    {
        if (penErr)
            *penErr = ERRTCPCONNCLOSED;
        return -1;
    }
    else; 

    //* 没有收到任何数据则立即返回0
    if (!l_stcbaInput[nInput].unRcvedBytes)
        return 0; 

    //* 将数据搬运到用户的接收缓冲区
    UINT unCpyBytes; 
    os_thread_mutex_lock(l_hMtxInput);
    {
        unCpyBytes = unDataBufSize > l_stcbaInput[nInput].unRcvedBytes ? l_stcbaInput[nInput].unRcvedBytes : unDataBufSize; 
        memcpy(pubDataBuf, l_stcbaInput[nInput].pubRcvBuf, unCpyBytes); 
        l_stcbaInput[nInput].unRcvedBytes = l_stcbaInput[nInput].unRcvedBytes - unCpyBytes;
        if (l_stcbaInput[nInput].unRcvedBytes)
            memmove(l_stcbaInput[nInput].pubRcvBuf, l_stcbaInput[nInput].pubRcvBuf + unCpyBytes, l_stcbaInput[nInput].unRcvedBytes); 

        //* 如果当前input绑定的协议为tcp，则立即更新接收窗口大小
        if (IPPROTO_TCP == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)        
            ((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->stLocal.usWndSize = l_stcbaInput[nInput].unRcvBufSize - l_stcbaInput[nInput].unRcvedBytes;
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return (INT)unCpyBytes; 
}

INT onps_input_recv_icmp(INT nInput, UCHAR **ppubPacket, UINT *punSrcAddr, UCHAR *pubTTL, INT nWaitSecs)
{
    if (nInput > SOCKET_NUM_MAX - 1)
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("onps_input_recv_icmp() failed, Handle %d is out of system scope\r\n", nInput);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return -1;
    }

    //* 超时，没有收到任何数据
    INT nRtnVal = os_thread_sem_pend(l_stcbaInput[nInput].hSem, nWaitSecs); 
    if (nRtnVal != 0)
    {
        if (nRtnVal < 0)
        {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
    #endif
            printf("onps_input_recv_icmp() failed, invalid semaphore, the input is %d\r\n", nInput);
    #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
            return -1; 
        }
        else
            return 0;
    }

    //* 报文继续上报给上层调用者
    PST_IP_HDR pstHdr = (PST_IP_HDR)l_stcbaInput[nInput].pubRcvBuf; 
    UCHAR usIpHdrLen = pstHdr->bitHdrLen * 4;
    *punSrcAddr = pstHdr->unSrcIP;
    *pubTTL = pstHdr->ubTTL; 
    *ppubPacket = l_stcbaInput[nInput].pubRcvBuf + usIpHdrLen;
    return (INT)l_stcbaInput[nInput].unRcvedBytes - usIpHdrLen;
}

const CHAR *onps_get_last_error(INT nInput, EN_ONPSERR *penErr)
{
    if (nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW; 

        return "The handle is out of system scope";
    }

    PSTCB_ONPS_INPUT pstcbInput = &l_stcbaInput[nInput];

    EN_ONPSERR enLastErr;
    os_critical_init();
    os_enter_critical();
    enLastErr = (EN_ONPSERR)pstcbInput->ubLastErr;
    os_exit_critical();

    if (penErr)
        *penErr = enLastErr;

    return onps_error(enLastErr);
}

void onps_set_last_error(INT nInput, EN_ONPSERR enErr)
{
    if (nInput > SOCKET_NUM_MAX - 1)
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("onps_set_last_error() failed, Handle %d is out of system scope\r\n", nInput);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }

    PSTCB_ONPS_INPUT pstcbInput = &l_stcbaInput[nInput];

    os_critical_init();
    os_enter_critical();
    pstcbInput->ubLastErr = (UCHAR)enErr;
    os_exit_critical();
}

BOOL onps_input_port_used(EN_IPPROTO enProtocol, USHORT usPort)
{
    BOOL blIsUsed = FALSE;
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList;
        PSTCB_ONPS_INPUT pstcbInput;
        while (pstNextNode)
        {
            pstcbInput = &l_stcbaInput[pstNextNode->uniData.nIndex];
            if (enProtocol == pstcbInput->ubIPProto && usPort == pstcbInput->uniHandle.stAddr.usPort)
            {
                blIsUsed = TRUE;
                break;
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return blIsUsed; 
}

USHORT onps_input_port_new(EN_IPPROTO enProtocol)
{
    USHORT usPort;

__lblPortNew:
    usPort = 65535 - (USHORT)(rand() % (TCPUDP_PORT_START + 1));

    //* 确定是否正在使用，如果未使用则没问题
    if (onps_input_port_used(enProtocol, usPort))
        goto __lblPortNew;
    else
        return usPort;
}

INT onps_input_get_handle(UINT unNetifIp, USHORT usPort)
{
    INT nInput = -1;
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList;
        PSTCB_ONPS_INPUT pstcbInput;
        while (pstNextNode)
        {
            pstcbInput = &l_stcbaInput[pstNextNode->uniData.nIndex];
            if (pstcbInput->ubIPProto == IPPROTO_TCP && pstcbInput->ubIPProto == IPPROTO_UDP)
            {
                if (unNetifIp == pstcbInput->uniHandle.stAddr.unNetifIp && usPort == pstcbInput->uniHandle.stAddr.usPort)
                {
                    nInput = pstNextNode->uniData.nIndex;
                    break; 
                }                
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return nInput;
}

INT onps_input_get_handle_ext(UINT unNetifIp, USHORT usPort, void *pvAttach)
{
    INT nInput = -1;
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList;
        PSTCB_ONPS_INPUT pstcbInput;
        while (pstNextNode)
        {
            pstcbInput = &l_stcbaInput[pstNextNode->uniData.nIndex];
            if (pstcbInput->ubIPProto == IPPROTO_TCP || pstcbInput->ubIPProto == IPPROTO_UDP)
            {
                if (unNetifIp == pstcbInput->uniHandle.stAddr.unNetifIp && usPort == pstcbInput->uniHandle.stAddr.usPort)
                {
                    nInput = pstNextNode->uniData.nIndex;
                    if (sizeof(pvAttach) == 4)
                        *((UINT *)pvAttach) = (UINT)pstcbInput->pvAttach;
                    else
                        *((ULONGLONG *)pvAttach) = (ULONGLONG)pstcbInput->pvAttach;
                    break;
                }
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return nInput;
}


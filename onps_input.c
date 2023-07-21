/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "one_shot_timer.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#define SYMBOL_GLOBALS
#include "onps_input.h"
#undef SYMBOL_GLOBALS
#include "netif/netif.h"
#include "ip/tcp_link.h"
#include "ip/udp_link.h"

#include "ip/icmp.h"

#if SUPPORT_IPV6
#include "ip/icmpv6.h"
#endif

//* 协议栈icmp/tcp/udp层接收处理节点
typedef struct _STCB_ONPS_INPUT_ {
    HSEM hSem;  //* 信号量，一旦收到数据，协议栈将投递信号量给上层接收者，避免上层调用者采用轮询读取数据的方式，避免CPU资源被过多占用    

    union {  //* 系统分配的接收者句柄，根据不同的上层协议其句柄信息均有所不同
        struct {
		#if SUPPORT_IPV6
			CHAR bFamily;  //* 协议族标识，这里用于区分底层协议族为ipv4还是ipv6
		#endif
            USHORT usIdentifier;
        } stIcmp; //* icmp层句柄

        ST_TCPUDP_HANDLE stTcpUdp; //* 句柄，使用IP地址和端口就可以唯一的标识一个tcp/udp通讯链路
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
    l_staNodes[0].uniData.nVal = 0;
    l_stcbaInput[0].hSem = INVALID_HSEM; 
    for (i = 1; i < SOCKET_NUM_MAX; i++)
    {
        l_staNodes[i - 1].pstNext = &l_staNodes[i];        
        l_stcbaInput[i].hSem = INVALID_HSEM;
        l_staNodes[i].uniData.nVal = i;
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

#if !SUPPORT_IPV6
INT onps_input_new(EN_IPPROTO enProtocol, EN_ONPSERR *penErr)
#else
INT onps_input_new(INT family, EN_IPPROTO enProtocol, EN_ONPSERR *penErr)
#endif
{
#if SUPPORT_IPV6
	if (AF_INET != family && AF_INET6 != family)
	{
		if (penErr)
			*penErr = ERRUNSUPPORTEDFAMILY;
		return -1;
	}
#endif

    HSEM hSem = os_thread_sem_init(0, 100000);
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
	case IPPROTO_ICMPv6: 
        unSize = ICMPRCVBUF_SIZE; 
        break; 

    case IPPROTO_TCP:
        unSize = TCPRCVBUF_SIZE; 
        break; 

    case IPPROTO_UDP:
        unSize = 0;
        break; 

    default:
        if (penErr)
            *penErr = ERRUNSUPPIPPROTO;
        return -1; 
    }

    UCHAR *pubRcvBuf = NULL;
    if (unSize)
    {
        pubRcvBuf = (UCHAR *)buddy_alloc(unSize, penErr);        
        if (NULL == pubRcvBuf)
        {
            os_thread_sem_uninit(hSem);
            return -1;
        }
    }    

    //* 申请一个input节点
    PST_SLINKEDLIST_NODE pstNode; 
    PSTCB_ONPS_INPUT pstcbInput; 
    os_thread_mutex_lock(l_hMtxInput); 
    {
        pstNode = sllist_get_node(&l_pstFreedSLList);
        pstcbInput = &l_stcbaInput[pstNode->uniData.nVal];
        pstcbInput->hSem = hSem; 
        pstcbInput->bRcvTimeout = -1; 
        pstcbInput->ubIPProto = (UCHAR)enProtocol; 
        pstcbInput->pubRcvBuf = pubRcvBuf; 
        pstcbInput->unRcvBufSize = unSize;
        pstcbInput->unRcvedBytes = 0; 
        pstcbInput->ubLastErr = ERRNO; 

		if (IPPROTO_ICMP == enProtocol
		#if SUPPORT_IPV6
			|| IPPROTO_ICMPv6 == enProtocol
		#endif
			)
		{
		#if SUPPORT_IPV6
			pstcbInput->uniHandle.stIcmp.bFamily = family;
		#endif
			pstcbInput->uniHandle.stIcmp.usIdentifier = 0;
		}
        else
        {
            pstcbInput->uniHandle.stTcpUdp.bType = TCP_TYPE_LCLIENT;
	#if SUPPORT_IPV6
			pstcbInput->uniHandle.stTcpUdp.bFamily = (CHAR)family;			
			memset(&pstcbInput->uniHandle.stTcpUdp.stSockAddr.uniIp, 0, sizeof(pstcbInput->uniHandle.stTcpUdp.stSockAddr.uniIp));
	#else
			pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv4 = 0;
	#endif
            pstcbInput->uniHandle.stTcpUdp.stSockAddr.usPort = 0;
            pstcbInput->pvAttach = NULL;
        }

        sllist_put_node(&l_pstInputSLList, pstNode); 
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return pstNode->uniData.nVal;
}

#if SUPPORT_ETHERNET
INT onps_input_new_tcp_remote_client(INT nInputSrv, USHORT usSrvPort, void *pvSrvIp, USHORT usCltPort, void *pvCltIp, PST_TCPLINK *ppstTcpLink, EN_ONPSERR *penErr)
{
    if (nInputSrv < 0 || nInputSrv > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;
        return -1;
    }

    //* 内存使用率超过70%就不再接受新的客户端连接，以确保原有链路的通讯不受到影响
    if ((FLOAT)0.7 < buddy_usage())
    {
        if (penErr)
            *penErr = ERRNOFREEMEM; 
        return -1;
    }

    //* 判断backlog队列是否已满
    PST_INPUTATTACH_TCPSRV pstAttach = l_stcbaInput[nInputSrv].pvAttach; 
    if (pstAttach)
    {
        if (pstAttach->usBacklogCnt >= pstAttach->usBacklogNum)
        {
            if (penErr)
                *penErr = ERRTCPBACKLOGFULL; 
            return -1;  
        }
    }
    else
    {
        if (penErr)
            *penErr = ERRTCPNOLISTEN;
        return -1; 
    }

    PST_TCPLINK pstLink = tcp_link_get(penErr);
    if (!pstLink)
        return -1;     

    PST_TCPBACKLOG pstBacklog = tcp_backlog_freed_get(penErr); 
    if (!pstBacklog)
    {
        tcp_link_free(pstLink);

        return -1; 
    }

    UINT unSize = TCPRCVBUF_SIZE;
    UCHAR *pubRcvBuf = (UCHAR *)buddy_alloc(unSize, penErr);
    if (NULL == pubRcvBuf)
    {
        tcp_link_free(pstLink); 
        tcp_backlog_free(pstBacklog); 

        return -1;
    }

    //* 申请一个input节点
    PST_SLINKEDLIST_NODE pstNode;
    PSTCB_ONPS_INPUT pstcbInput;
    os_thread_mutex_lock(l_hMtxInput);
    {
        pstNode = sllist_get_node(&l_pstFreedSLList);
        pstcbInput = &l_stcbaInput[pstNode->uniData.nVal];
        pstcbInput->hSem = l_stcbaInput[nInputSrv].hSem; 
        pstcbInput->bRcvTimeout = 0; //* 实际的数据读取函数recv()不再等待，因为客户一旦调用recv()函数，说明已经确定收到数据了（由tcp服务器提供的poll函数来通知用户数据已到达）
        pstcbInput->ubIPProto = (UCHAR)IPPROTO_TCP;
        pstcbInput->pubRcvBuf = pubRcvBuf;
        pstcbInput->unRcvBufSize = unSize;
        pstcbInput->unRcvedBytes = 0;
        pstcbInput->uniHandle.stTcpUdp.bType = TCP_TYPE_RCLIENT;        
	#if SUPPORT_IPV6         
        pstcbInput->uniHandle.stTcpUdp.bFamily = l_stcbaInput[nInputSrv].uniHandle.stTcpUdp.bFamily;
		if (AF_INET == l_stcbaInput[nInputSrv].uniHandle.stTcpUdp.bFamily)        
			pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv4 = *((in_addr_t *)pvSrvIp);
        else                               
            memcpy(pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv6, (UCHAR *)pvSrvIp, 16);        		
	#else
		pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv4 = *((in_addr_t *)pvSrvIp);
	#endif
        pstcbInput->uniHandle.stTcpUdp.stSockAddr.usPort = usSrvPort;
        pstcbInput->pvAttach = pstLink;

        pstLink->bState = TLSRCVEDSYN;

        pstLink->stLocal.usWndSize = pstcbInput->unRcvBufSize;
        pstLink->stLocal.bIsZeroWnd = FALSE;
        pstLink->stLocal.pstHandle = &pstcbInput->uniHandle.stTcpUdp;
                
	#if SUPPORT_IPV6        
		if (AF_INET == l_stcbaInput[nInputSrv].uniHandle.stTcpUdp.bFamily)
			pstLink->stPeer.stSockAddr.saddr_ipv4 = *((in_addr_t *)pvCltIp);
        else                    
            memcpy(pstLink->stPeer.stSockAddr.saddr_ipv6, (UCHAR *)pvCltIp, 16);        
	#else
        pstLink->stPeer.stSockAddr.saddr_ipv4 = *((in_addr_t *)pvCltIp);
	#endif
        pstLink->stPeer.stSockAddr.usPort = usCltPort;      

        pstLink->stcbWaitAck.nInput = pstNode->uniData.nVal; 
        pstLink->stcbWaitAck.bRcvTimeout = 1; 

        pstBacklog->nInput = pstNode->uniData.nVal; 
	#if SUPPORT_IPV6
		if (AF_INET == l_stcbaInput[nInputSrv].uniHandle.stTcpUdp.bFamily)
			pstBacklog->stAddr.saddr_ipv4 = *(in_addr_t *)pvCltIp;
        else        
            memcpy(pstBacklog->stAddr.saddr_ipv6, (UCHAR *)pvCltIp, 16);
	#else
        pstBacklog->stAddr.unIp = *((in_addr_t *)pvCltIp);
	#endif
        pstBacklog->stAddr.usPort = usCltPort; 
        pstLink->pstBacklog = pstBacklog;

        pstLink->nInputSrv = nInputSrv; 

        sllist_put_node(&l_pstInputSLList, pstNode);
    }
    os_thread_mutex_unlock(l_hMtxInput);

    *ppstTcpLink = pstLink; 

    return pstNode->uniData.nVal;
}
#endif

void onps_input_free(INT nInput)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
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
            if (pstNextNode->uniData.nVal == nInput)
            {
                PSTCB_ONPS_INPUT pstcbInput = &l_stcbaInput[nInput];

                //* 释放tcp/udp协议附加的保存通讯链路相关信息的数据节点
                if (pstcbInput->pvAttach)
                {
                    if (IPPROTO_TCP == pstcbInput->ubIPProto)
                    {
                    #if SUPPORT_ETHERNET
                        if(TCP_TYPE_SERVER == pstcbInput->uniHandle.stTcpUdp.bType)
                            tcpsrv_input_attach_free((PST_INPUTATTACH_TCPSRV)pstcbInput->pvAttach); 
                        else                    
                            tcp_link_free((PST_TCPLINK)pstcbInput->pvAttach);
                    #else
                        tcp_link_free((PST_TCPLINK)pstcbInput->pvAttach);
                    #endif
                    }
                }

                //* 先释放申请的相关资源
                if (pstcbInput->pubRcvBuf)
                {
                    buddy_free(pstcbInput->pubRcvBuf);
                    pstcbInput->pubRcvBuf = NULL;
                }
                if (INVALID_HSEM != pstcbInput->hSem)
                {
                    if (TCP_TYPE_RCLIENT != pstcbInput->uniHandle.stTcpUdp.bType) //* 服务器和本地客户端（连接远端tcp服务器）用到了semaphore，所以这里需要释放掉
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
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;

        return FALSE;
    }

    PSTCB_ONPS_INPUT pstcbInput = &l_stcbaInput[nInput];
    switch (enInputOpt)
    {    
    case IOPT_SETICMPECHOID:
        if (IPPROTO_ICMP == (EN_IPPROTO)pstcbInput->ubIPProto || IPPROTO_ICMPv6 == (EN_IPPROTO)pstcbInput->ubIPProto)
            pstcbInput->uniHandle.stIcmp.usIdentifier = *((USHORT *)pvVal); 
        else
            goto __lblIpProtoNotMatched;
        break;

    case IOPT_SETTCPUDPADDR:
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto || IPPROTO_UDP == (EN_IPPROTO)pstcbInput->ubIPProto)
        {
            pstcbInput->uniHandle.stTcpUdp = *((PST_TCPUDP_HANDLE)pvVal);

            //* tcp服务器不需要接收缓冲区，每个远程客户端均单独分配了一个接收缓冲区，所以这个分支的作用就是释放掉input模块申请的缓冲区
            if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto && TCP_TYPE_SERVER == pstcbInput->uniHandle.stTcpUdp.bType)
            {
                if (pstcbInput->pubRcvBuf)
                {
                    buddy_free(pstcbInput->pubRcvBuf); 
                    pstcbInput->pubRcvBuf = NULL; 
                }
            }
        }
        else
            goto __lblIpProtoNotMatched; 
        break; 

    case IOPT_FREETCPSRVRCVBUF:
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto && TCP_TYPE_SERVER == pstcbInput->uniHandle.stTcpUdp.bType)
        {
            if (pstcbInput->pubRcvBuf)
            {
                buddy_free(pstcbInput->pubRcvBuf);
                pstcbInput->pubRcvBuf = NULL;                 
            }
        }
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
            if (TCP_TYPE_SERVER != pstcbInput->uniHandle.stTcpUdp.bType)
            {
                ((PST_TCPLINK)pstcbInput->pvAttach)->stLocal.usWndSize = pstcbInput->unRcvBufSize;
                ((PST_TCPLINK)pstcbInput->pvAttach)->stLocal.bIsZeroWnd = FALSE;
                ((PST_TCPLINK)pstcbInput->pvAttach)->stLocal.pstHandle = &pstcbInput->uniHandle.stTcpUdp;
                ((PST_TCPLINK)pstcbInput->pvAttach)->stcbWaitAck.bRcvTimeout = -1/*pstcbInput->bRcvTimeout*/; //* 缺省所有发送操作都需要等待对端的应答
                ((PST_TCPLINK)pstcbInput->pvAttach)->stcbWaitAck.nInput = nInput; 
            }            
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
        /*
        if (IPPROTO_UDP == (EN_IPPROTO)pstcbInput->ubIPProto || (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto && TCP_TYPE_SERVER != pstcbInput->uniHandle.stAddr.bType))
        {
            ((PST_TCPLINK)pstcbInput->pvAttach)->stcbWaitAck.bRcvTimeout = pstcbInput->bRcvTimeout;
        }
        */
        break; 

    case IOPT_SETTCPLINKFLAGS:
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto)
        {
            if (NULL == pstcbInput->pvAttach)
            {
                if (penErr)
                    *penErr = ERRTCPNOTCONNECTED;
                return FALSE;
            }

            if (TCP_TYPE_SERVER != pstcbInput->uniHandle.stTcpUdp.bType)
                ((PST_TCPLINK)pstcbInput->pvAttach)->uniFlags.usVal = *((USHORT *)pvVal); 
        }
        else
            goto __lblIpProtoNotMatched;

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
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
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
                *((UINT *)pvVal) = (UINT)&pstcbInput->uniHandle.stTcpUdp;
            else
                *((ULONGLONG *)pvVal) = (ULONGLONG)&pstcbInput->uniHandle.stTcpUdp;
        }
        else
        {
            if (penErr)
                *penErr = ERRIPROTOMATCH;
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

    case IOPT_GETTCPUDPLINK: 
        if (sizeof(pvVal) == 4)
        {
            if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto && TCP_TYPE_SERVER == pstcbInput->uniHandle.stTcpUdp.bType)
                *((UINT *)pvVal) = 0/*(UINT)((PST_INPUTATTACH_TCPSRV)pstcbInput->pvAttach)->pstClients*/;
            else
                *((UINT *)pvVal) = (UINT)pstcbInput->pvAttach;
        }
        else
        {
            if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto && TCP_TYPE_SERVER == pstcbInput->uniHandle.stTcpUdp.bType)
                *((ULONGLONG *)pvVal) = 0/*(ULONGLONG)((PST_INPUTATTACH_TCPSRV)pstcbInput->pvAttach)->pstClients*/; 
            else
                *((ULONGLONG *)pvVal) = (ULONGLONG)pstcbInput->pvAttach;
        }
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
                *((USHORT *)pvVal) = ((PST_TCPLINK)(pstcbInput->pvAttach))->stcbWaitAck.usSendDataBytes; 
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

    case IOPT_GETTCPLINKFLAGS: 
        if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto)
        {
            if (pstcbInput->pvAttach)
            {
                if (penErr)
                    *penErr = ERRTCPNOTCONNECTED;
                return FALSE;
            }

            if (TCP_TYPE_SERVER != pstcbInput->uniHandle.stTcpUdp.bType)
                *((USHORT *)pvVal) = ((PST_TCPLINK)pstcbInput->pvAttach)->uniFlags.usVal;
        }
        else
        {
            if (penErr)
                *penErr = ERRIPROTOMATCH;
            return FALSE;
        }

        break; 

#if SUPPORT_IPV6
	case IOPT_GETICMPAF:
		if (IPPROTO_ICMP == (EN_IPPROTO)pstcbInput->ubIPProto || IPPROTO_ICMPv6 == (EN_IPPROTO)pstcbInput->ubIPProto)
		{
			*((CHAR *)pvVal) = pstcbInput->uniHandle.stIcmp.bFamily; 
		}
		else
		{
			if (penErr)
				*penErr = ERRIPROTOMATCH;
			return FALSE;
		}
		break; 
#endif    

    default:
        if (penErr)
            *penErr = ERRUNSUPPIOPT;
        return FALSE;
    }

    return TRUE;
}

void onps_input_sem_post(INT nInput)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
        return; 
    
    if (INVALID_HSEM != l_stcbaInput[nInput].hSem/* && l_stcbaInput[nInput].bRcvTimeout*/)
        os_thread_sem_post(l_stcbaInput[nInput].hSem); 
}

INT onps_input_sem_pend(INT nInput, INT nWaitSecs, EN_ONPSERR *penErr)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;
        return -1;
    }

    if (INVALID_HSEM != l_stcbaInput[nInput].hSem && l_stcbaInput[nInput].bRcvTimeout)
    {
        INT nRtnVal = os_thread_sem_pend(l_stcbaInput[nInput].hSem, nWaitSecs); 
        if (nRtnVal < 0)
        {
            if (penErr)
                *penErr = ERRINVALIDSEM;
        }
        return nRtnVal; 
    }

    return 0; 
}

INT onps_input_sem_pend_uncond(INT nInput, INT nWaitSecs, EN_ONPSERR *penErr)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;
        return -1;
    }

    if (INVALID_HSEM != l_stcbaInput[nInput].hSem)
    {
        INT nRtnVal = os_thread_sem_pend(l_stcbaInput[nInput].hSem, nWaitSecs);
        if (nRtnVal < 0)
        {
            if (penErr)
                *penErr = ERRINVALIDSEM;
        }
        return nRtnVal;
    }

    return 0;
}

#if SUPPORT_ETHERNET
void onps_input_sem_post_tcpsrv_accept(INT nSrvInput, INT nCltInput, UINT unLocalSeqNum)
{
    PST_INPUTATTACH_TCPSRV pstAttach; 
    PST_TCPLINK pstLink; 

    if (nSrvInput < 0 || nSrvInput > SOCKET_NUM_MAX - 1 || nCltInput < 0 || nCltInput > SOCKET_NUM_MAX - 1)
        goto __lblErr; 

    //* 如果不是tcp协议或者并不是tcp服务器链路则不投递tcp链路已建立信号量
    if (IPPROTO_TCP != (EN_IPPROTO)l_stcbaInput[nSrvInput].ubIPProto || TCP_TYPE_SERVER != l_stcbaInput[nSrvInput].uniHandle.stTcpUdp.bType)
        goto __lblErr;

    //* 取出已经准备好的backlog，加入请求队列然后投递给accept()用户
    pstAttach = (PST_INPUTATTACH_TCPSRV)l_stcbaInput[nSrvInput].pvAttach;
    pstLink = (PST_TCPLINK)l_stcbaInput[nCltInput].pvAttach;
    if (pstAttach && pstLink)
    {
        pstLink->stcbWaitAck.bIsAcked = TRUE;
        one_shot_timer_safe_free(pstLink->stcbWaitAck.pstTimer);

        //* 状态迁移到“已连接”
        pstLink->stLocal.unSeqNum = unLocalSeqNum; 
        pstLink->bState = TLSCONNECTED;

        tcp_backlog_put(&pstAttach->pstSListBacklog, pstLink->pstBacklog, &pstAttach->usBacklogCnt); 
        os_thread_sem_post(pstAttach->hSemAccept);

        return; 
    }    

__lblErr: 
    onps_input_free(nCltInput); 
}
#endif

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
            pstcbInput = &l_stcbaInput[pstNextNode->uniData.nVal];
            if ((IPPROTO_ICMP == (EN_IPPROTO)pstcbInput->ubIPProto || IPPROTO_ICMPv6 == (EN_IPPROTO)pstcbInput->ubIPProto) 
				&& usIdentifier == pstcbInput->uniHandle.stIcmp.usIdentifier)
            {
                nInput = pstNextNode->uniData.nVal;
                break;
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return nInput; 
}

#if SUPPORT_IPV6
BOOL onps_input_recv(INT nInput, const UCHAR *pubData, INT nDataBytes, void *pvFromIP, USHORT usFromPort, EN_ONPSERR *penErr)
#else
BOOL onps_input_recv(INT nInput, const UCHAR *pubData, INT nDataBytes, in_addr_t unFromIP, USHORT usFromPort, EN_ONPSERR *penErr)
#endif
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;

        return FALSE; 
    }
    
	//* 这个接收函数仅用于icmp、icmpv6及udp协议的接收，其它协议不处理
    if (IPPROTO_ICMP != (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto && IPPROTO_ICMPv6 != (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto && IPPROTO_UDP != (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)
    {
		if (penErr)
			*penErr = ERRUNSUPPIPPROTO;

		return FALSE;
    }    

    //* icmp报文只要是到达就直接覆盖前一组，无论前一组报文是否已被读取
    if (IPPROTO_ICMP == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto || IPPROTO_ICMPv6 == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)
    {        
        UINT unCpyBytes = (UINT)nDataBytes < l_stcbaInput[nInput].unRcvBufSize ? (UINT)nDataBytes : l_stcbaInput[nInput].unRcvBufSize; 
        memcpy(l_stcbaInput[nInput].pubRcvBuf, pubData, unCpyBytes);
        l_stcbaInput[nInput].unRcvedBytes = unCpyBytes;      

        //* 投递信号给上层用户，告知对端数据已到达
        if (l_stcbaInput[nInput].bRcvTimeout)
            os_thread_sem_post(l_stcbaInput[nInput].hSem);

        return TRUE; 
    }

    //* 执行到这里就可以确定这是udp报文了，所以直接将数据搬运到udp链路的接收缓冲区
    BOOL blIsOK = TRUE; 
    os_thread_mutex_lock(l_hMtxInput);
    {
	#if SUPPORT_IPV6
		PST_TCPUDP_HANDLE pstHandle = &l_stcbaInput[nInput].uniHandle.stTcpUdp;
	#endif
		UINT unSize = sizeof(ST_RCVED_UDP_PACKET) + nDataBytes;
		UCHAR *pubRcvedPacket = (UCHAR *)buddy_alloc(unSize, penErr);
		if (pubRcvedPacket)
		{
			//* 填充数据到节点上
			PST_RCVED_UDP_PACKET pstRcvedPacket = (PST_RCVED_UDP_PACKET)pubRcvedPacket;
			pstRcvedPacket->usLen = (USHORT)nDataBytes;					
		#if SUPPORT_IPV6
			if (AF_INET6 == pstHandle->bFamily)			
				memcpy(pstRcvedPacket->stSockAddr.saddr_ipv6, (UCHAR *)pvFromIP, 16); 			
			else
				pstRcvedPacket->stSockAddr.saddr_ipv4 = *((UINT *)pvFromIP);
		#else			
			pstRcvedPacket->stSockAddr.saddr_ipv4 = unFromIP;
		#endif
			pstRcvedPacket->stSockAddr.usPort = usFromPort;
			pstRcvedPacket->pstNext = NULL;
			memcpy(pubRcvedPacket + sizeof(ST_RCVED_UDP_PACKET), pubData, nDataBytes);

			//* 挂接到主链表上
			PST_RCVED_UDP_PACKET pstRcvedPacketLink = (PST_RCVED_UDP_PACKET)l_stcbaInput[nInput].pubRcvBuf;
			if (pstRcvedPacketLink)
			{
				PST_RCVED_UDP_PACKET pstNextPacket = pstRcvedPacketLink;
				while (pstNextPacket)
				{
					if (!pstNextPacket->pstNext)
					{
						pstNextPacket->pstNext = pstRcvedPacket;
						break;
					}
					pstNextPacket = pstNextPacket->pstNext;
				}
			}
			else
				l_stcbaInput[nInput].pubRcvBuf = (UCHAR *)pstRcvedPacket;
		}
		else
			blIsOK = FALSE;
    }
    os_thread_mutex_unlock(l_hMtxInput);    

    //* 搬运成功则投递信号给上层用户，告知对端数据已到达
    if (blIsOK)
    {       		
        //* 只要指定了接收等待时间，这里就需要投递一个semaphore用于标准的poll操作
		if (l_stcbaInput[nInput].bRcvTimeout)
			os_thread_sem_post(l_stcbaInput[nInput].hSem); 
    }    

    return blIsOK; 
}

INT onps_input_tcp_recv(INT nInput, const UCHAR *pubData, INT nDataBytes, EN_ONPSERR *penErr)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;

        return -1; 
    }

    if (IPPROTO_TCP != (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)
    {
        if (penErr)
            *penErr = ERRIPROTOMATCH;

        return -1;        
    }

    //* TCP层进入FIN操作后如果还有数据到来，依然会调用该搬运函数，只不过入口参数pubData为NULL，如此操作的原因是确保接收窗口不影响对端数据发送，虽然
    //* 本地已经关闭了发送，且到达的数据会被丢弃（主要是看着wireshark上的0窗口报警心烦）
    if (!pubData)
    {
        ((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->stLocal.usWndSize = l_stcbaInput[nInput].unRcvBufSize;
        return 0;
    }

    //* 将数据搬运到接收缓冲区
    BOOL blIsOK = TRUE; 
    UINT unCpyBytes = 0; 
    os_thread_mutex_lock(l_hMtxInput);
    {
#if SUPPORT_ETHERNET
        INT nInputSrv; 
        PST_INPUTATTACH_TCPSRV pstAttachTcpSrv = NULL; 
        PST_TCPSRV_RCVQUEUE_NODE pstRcvQueueNode = NULL;
        if (TCP_TYPE_RCLIENT == l_stcbaInput[nInput].uniHandle.stTcpUdp.bType)
        {
            nInputSrv = ((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->nInputSrv; 
            pstAttachTcpSrv = (PST_INPUTATTACH_TCPSRV)l_stcbaInput[nInputSrv].pvAttach;

            //* 如果当前tcp链路是一个远端tcp客户端且服务器的接收模式采用poll模型，则需要先申请一个接收队列节点
            if (TCPSRVRCVMODE_POLL == pstAttachTcpSrv->bRcvMode)
            {
                if (NULL == (pstRcvQueueNode = tcpsrv_recv_queue_freed_get(penErr)))
                    blIsOK = FALSE; 
            }                      
        }
#endif

        if (blIsOK)
        {
            unCpyBytes = l_stcbaInput[nInput].unRcvBufSize - l_stcbaInput[nInput].unRcvedBytes;
            //* 对于TCP协议由于存在流控（滑动窗口机制），理论上收到的数据应该一直小于等于缓冲区剩余容量才对，即unCpyBytes一直大于等于nDataBytes
            //* 对于UDP协议则存在unCpyBytes小于nDataBytes的情况，此时只能丢弃剩余无法搬运的数据了
            unCpyBytes = unCpyBytes > (UINT)nDataBytes ? (UINT)nDataBytes : unCpyBytes;
            memcpy(l_stcbaInput[nInput].pubRcvBuf + l_stcbaInput[nInput].unRcvedBytes, pubData, unCpyBytes);
            l_stcbaInput[nInput].unRcvedBytes += unCpyBytes;

            //* 如果当前input绑定的协议为tcp，则立即更新接收窗口大小        
            ((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->stLocal.usWndSize = (USHORT)(l_stcbaInput[nInput].unRcvBufSize - l_stcbaInput[nInput].unRcvedBytes);             
            if (!((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->stLocal.usWndSize)
                ((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->stLocal.bIsZeroWnd = TRUE;

        #if SUPPORT_ETHERNET
            //* 如果接收队列不为NULL，则需要投递这个到达的数据到服务器接收队列
            if (pstRcvQueueNode)                                    
                tcpsrv_recv_queue_put(&pstAttachTcpSrv->pstSListRcvQueue, pstRcvQueueNode, nInput);             
        #endif
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);

    //* 搬运成功则投递信号给上层用户，告知对端数据已到达
    if (blIsOK)
    {
        //* 在这里，除了本地客户端显式地指定需要等待数据到达semaphore之外，tcp远端客户端也会投递一个semaphore用于标准的poll操作
        if (l_stcbaInput[nInput].bRcvTimeout || TCP_TYPE_RCLIENT == l_stcbaInput[nInput].uniHandle.stTcpUdp.bType)
            os_thread_sem_post(l_stcbaInput[nInput].hSem);

        return (INT)unCpyBytes; 
    }
    else
        return -1; 
}

INT onps_input_recv_upper(INT nInput, UCHAR *pubDataBuf, UINT unDataBufSize, void *pvFromIP, USHORT *pusFromPort, EN_ONPSERR *penErr)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
    {
        if (penErr)
            *penErr = ERRINPUTOVERFLOW;

        return -1;
    }
    
	//* tcp协议并且已经进入复位或处于关闭过程中则直接返回，不再搬运数据
    if (IPPROTO_TCP == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)
    {
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
    }    

    //* 没有收到任何数据则立即返回0
    if (IPPROTO_TCP == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto && !l_stcbaInput[nInput].unRcvedBytes)
        return 0; 

    //* 将数据搬运到用户的接收缓冲区
    INT nRtnVal = -1;
    UINT unCpyBytes; 
    os_thread_mutex_lock(l_hMtxInput);
    {
        if (IPPROTO_TCP == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)
        {
            unCpyBytes = unDataBufSize > l_stcbaInput[nInput].unRcvedBytes ? l_stcbaInput[nInput].unRcvedBytes : unDataBufSize;
            memcpy(pubDataBuf, l_stcbaInput[nInput].pubRcvBuf, unCpyBytes);
            l_stcbaInput[nInput].unRcvedBytes = l_stcbaInput[nInput].unRcvedBytes - unCpyBytes;
            if (l_stcbaInput[nInput].unRcvedBytes)
                memmove(l_stcbaInput[nInput].pubRcvBuf, l_stcbaInput[nInput].pubRcvBuf + unCpyBytes, l_stcbaInput[nInput].unRcvedBytes);

            //* 如果当前input绑定的协议为tcp，则立即更新接收窗口大小        
            ((PST_TCPLINK)l_stcbaInput[nInput].pvAttach)->stLocal.usWndSize = l_stcbaInput[nInput].unRcvBufSize - l_stcbaInput[nInput].unRcvedBytes;             
            nRtnVal = (INT)unCpyBytes;
        }
        else if (IPPROTO_UDP == (EN_IPPROTO)l_stcbaInput[nInput].ubIPProto)
        {
		#if SUPPORT_IPV6
			PST_TCPUDP_HANDLE pstHandle = &l_stcbaInput[nInput].uniHandle.stTcpUdp;
		#endif

            //* 从主链表获取数据
            PST_RCVED_UDP_PACKET pstRcvedPacketLink = (PST_RCVED_UDP_PACKET)l_stcbaInput[nInput].pubRcvBuf;            
            if (pstRcvedPacketLink)
            {
                PST_RCVED_UDP_PACKET pstRcvedPacket = pstRcvedPacketLink;
                unCpyBytes = unDataBufSize > (UINT)pstRcvedPacket->usLen ? (UINT)pstRcvedPacket->usLen : unDataBufSize;
                memcpy(pubDataBuf, l_stcbaInput[nInput].pubRcvBuf + sizeof(ST_RCVED_UDP_PACKET), unCpyBytes);

				if (pvFromIP)
				{
				#if SUPPORT_IPV6
					if (AF_INET6 == pstHandle->bFamily)
						memcpy((UCHAR *)pvFromIP, pstRcvedPacket->stSockAddr.saddr_ipv6, 16); 
					else
						*((in_addr_t *)pvFromIP) = pstRcvedPacket->stSockAddr.saddr_ipv4;
				#else
                    *((in_addr_t *)pvFromIP) = pstRcvedPacket->stSockAddr.saddr_ipv4;
				#endif
				}
                if (pusFromPort)
                    *pusFromPort = pstRcvedPacket->stSockAddr.usPort;

                //* 移动到下一个报文节点并释放当前占用的内存
                l_stcbaInput[nInput].pubRcvBuf = (UCHAR *)pstRcvedPacketLink->pstNext;
                buddy_free(pstRcvedPacket);

                nRtnVal = (INT)unCpyBytes;
            }
            else
                nRtnVal = 0;
        }
        else
        {
            if (penErr)
                *penErr = ERRIPROTOMATCH;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return nRtnVal;
}

INT onps_input_recv_icmp(INT nInput, UCHAR **ppubPacket, void *pvSrcAddr, UCHAR *pubTTL, UCHAR *pubType, UCHAR *pubCode, INT nWaitSecs, EN_ONPSERR *penErr)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
    {
		if (penErr)
			*penErr = ERRINPUTOVERFLOW; 
        return -1;
    }

    //* 超时，没有收到任何数据
    INT nRtnVal = os_thread_sem_pend(l_stcbaInput[nInput].hSem, nWaitSecs); 
    if (nRtnVal != 0)
    {
        if (nRtnVal < 0)
        {
			if (penErr)
				*penErr = ERRINVALIDSEM; 
            return -1; 
        }
        else
            return 0;
    }

	//* 报文继续上报给上层调用者
	UCHAR usIpHdrLen; 
#if SUPPORT_IPV6
	if (AF_INET == l_stcbaInput[nInput].uniHandle.stIcmp.bFamily)
	{
		PST_IP_HDR pstHdr = (PST_IP_HDR)l_stcbaInput[nInput].pubRcvBuf;
		usIpHdrLen = pstHdr->bitHdrLen * 4;
		PST_ICMP_HDR pstIcmpHdr = (PST_ICMP_HDR)(l_stcbaInput[nInput].pubRcvBuf + usIpHdrLen);
		if (ICMP_ECHOREPLY == pstIcmpHdr->ubType || ICMP_ROUTEADVERT == pstIcmpHdr->ubType)
		{
			if (pvSrcAddr)
				*((in_addr_t *)pvSrcAddr) = pstHdr->unSrcIP;
			if (pubTTL)
				*pubTTL = pstHdr->ubTTL;
		}
		else
		{
			if (pubType)
				*pubType = pstIcmpHdr->ubType; 

			if (pubCode)
				*pubCode = pstIcmpHdr->ubCode; 

			return -1; 
		}
	}
	else
	{
		PST_IPv6_HDR pstHdr = (PST_IPv6_HDR)l_stcbaInput[nInput].pubRcvBuf;
		usIpHdrLen = sizeof(ST_IPv6_HDR);
		PST_ICMPv6_HDR pstIcmpHdr = (PST_ICMPv6_HDR)(l_stcbaInput[nInput].pubRcvBuf + usIpHdrLen);
		if (pstIcmpHdr->ubType > ICMPv6_ERRPP)
		{
			if (pvSrcAddr)
				memcpy((UCHAR *)pvSrcAddr, pstHdr->ubaSrcIpv6, 16);
		}		
		else
		{
			if (pubType)
				*pubType = pstIcmpHdr->ubType;

			if (pubCode)
				*pubCode = pstIcmpHdr->ubCode;

			return -1;
		}
	}
#else    
    PST_IP_HDR pstHdr = (PST_IP_HDR)l_stcbaInput[nInput].pubRcvBuf; 
    usIpHdrLen = pstHdr->bitHdrLen * 4;
	if (pvSrcAddr)	
        *((in_addr_t *)pvSrcAddr) = pstHdr->unSrcIP;
    if(pubTTL)
        *pubTTL = pstHdr->ubTTL; 
#endif
    *ppubPacket = l_stcbaInput[nInput].pubRcvBuf + usIpHdrLen;
    return (INT)l_stcbaInput[nInput].unRcvedBytes - usIpHdrLen;
}

const CHAR *onps_get_last_error(INT nInput, EN_ONPSERR *penErr)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
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

EN_ONPSERR onps_get_last_error_code(INT nInput)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)    
        return ERRINPUTOVERFLOW;

    PSTCB_ONPS_INPUT pstcbInput = &l_stcbaInput[nInput];

    EN_ONPSERR enLastErr;
    os_critical_init();
    os_enter_critical();
    {
        enLastErr = (EN_ONPSERR)pstcbInput->ubLastErr; 
    }    
    os_exit_critical(); 

    return enLastErr; 
}

void onps_set_last_error(INT nInput, EN_ONPSERR enErr)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
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

#if SUPPORT_IPV6
BOOL onps_input_port_used(INT nFamily, EN_IPPROTO enProtocol, USHORT usPort)
#else
BOOL onps_input_port_used(EN_IPPROTO enProtocol, USHORT usPort)
#endif
{
    BOOL blIsUsed = FALSE;
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList;
        PSTCB_ONPS_INPUT pstcbInput;
        while (pstNextNode)
        {
            pstcbInput = &l_stcbaInput[pstNextNode->uniData.nVal];
		#if SUPPORT_IPV6
            if ((CHAR)nFamily == pstcbInput->uniHandle.stTcpUdp.bFamily && enProtocol == pstcbInput->ubIPProto && usPort == pstcbInput->uniHandle.stTcpUdp.stSockAddr.usPort)
		#else
			if (enProtocol == pstcbInput->ubIPProto && usPort == pstcbInput->uniHandle.stTcpUdp.stSockAddr.usPort)
		#endif
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

#if SUPPORT_IPV6
USHORT onps_input_port_new(INT nFamily, EN_IPPROTO enProtocol)
#else
USHORT onps_input_port_new(EN_IPPROTO enProtocol)
#endif
{
    USHORT usPort;

__lblPortNew:
    usPort = 65535 - (USHORT)(rand() % (TCPUDP_PORT_START + 1));

    //* 确定是否正在使用，如果未使用则没问题
#if SUPPORT_IPV6
    if (onps_input_port_used(nFamily, enProtocol, usPort))
#else
	if (onps_input_port_used(enProtocol, usPort))
#endif
        goto __lblPortNew;
    else
        return usPort;
}

#if SUPPORT_ETHERNET
INT onps_input_get_handle_of_tcp_rclient(void *pvSrvIp, USHORT usSrvPort, void *pvCltIp, USHORT usCltPort, PST_TCPLINK *ppstTcpLink)
{
    INT nInput = -1;
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList;
        PSTCB_ONPS_INPUT pstcbInput;
        while (pstNextNode)
        {
            pstcbInput = &l_stcbaInput[pstNextNode->uniData.nVal];
            if (IPPROTO_TCP == pstcbInput->ubIPProto && TCP_TYPE_RCLIENT == pstcbInput->uniHandle.stTcpUdp.bType && pstcbInput->pvAttach)
            {                
                PST_TCPLINK pstLink = (PST_TCPLINK)pstcbInput->pvAttach; 
#if SUPPORT_IPV6
				BOOL blIsSrvIpMatched, blIsCltIpMatched; 
				if (AF_INET == pstcbInput->uniHandle.stTcpUdp.bFamily)
				{
					blIsSrvIpMatched = (BOOL)(*((in_addr_t *)pvSrvIp) == pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv4);
					blIsCltIpMatched = (BOOL)(*((in_addr_t *)pvCltIp) == pstLink->stPeer.stSockAddr.saddr_ipv4);
				}
				else
				{
					blIsSrvIpMatched = (BOOL)(!memcmp((UCHAR *)pvSrvIp, pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv6, 16));
					blIsCltIpMatched = (BOOL)(!memcmp((UCHAR *)pvCltIp, pstLink->stPeer.stSockAddr.saddr_ipv6, 16));
				}

				if (blIsSrvIpMatched
					&& usSrvPort == pstcbInput->uniHandle.stTcpUdp.stSockAddr.usPort
					&& blIsCltIpMatched
					&& usCltPort == pstLink->stPeer.stSockAddr.usPort)
#else
                if (*((in_addr_t *)pvSrvIp) == pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv4
                    && usSrvPort == pstcbInput->uniHandle.stTcpUdp.stSockAddr.usPort
                    && *((in_addr_t *)pvCltIp) == pstLink->stPeer.stSockAddr.saddr_ipv4
                    && usCltPort == pstLink->stPeer.stSockAddr.usPort)
#endif
                {
                    *ppstTcpLink = pstLink; 
                    nInput = pstNextNode->uniData.nVal;
                    break; 
                }                
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return nInput;
}
#endif

#if SUPPORT_IPV6
INT onps_input_get_handle(INT family, EN_IPPROTO enIpProto, void *pvNetifIp, USHORT usPort, void *pvAttach)
#else
INT onps_input_get_handle(EN_IPPROTO enIpProto, UINT unNetifIp, USHORT usPort, void *pvAttach)
#endif
{
    INT nInput = -1;
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList;
        PSTCB_ONPS_INPUT pstcbInput;
        while (pstNextNode)
        {
            pstcbInput = &l_stcbaInput[pstNextNode->uniData.nVal];
    
            if (enIpProto == pstcbInput->ubIPProto 
        #if SUPPORT_IPV6
                && family == pstcbInput->uniHandle.stTcpUdp.bFamily
        #endif
                )
            {                
			#if SUPPORT_IPV6
				BOOL blIsMatched; 
				if (AF_INET6 == pstcbInput->uniHandle.stTcpUdp.bFamily)
					blIsMatched = (BOOL)(!memcmp((UCHAR *)pvNetifIp, pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv6, 16));
				else
					blIsMatched = (BOOL)(*((UINT *)pvNetifIp) == pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv4);
			#endif

                //* 端口必须匹配，地址则根据不同情况另说
                if (usPort == pstcbInput->uniHandle.stTcpUdp.stSockAddr.usPort)
                {
					//* 即使ipv6，前4个字节为0也可以认为未绑定任何地址，当前链路默认绑定所有网络接口；如果指定地址则必须完全匹配					
                    if (pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv4 == 0 || 
				#if SUPPORT_IPV6
						blIsMatched) 
				#else
						unNetifIp == pstcbInput->uniHandle.stTcpUdp.stSockAddr.saddr_ipv4)
				#endif
                    {
                        //* 如果是tcp协议，我们只需找出本地tcp客户端和tcp服务器（TCP_TYPE_SERVER、TCP_TYPE_LCLIENT）类型的input节点，连接本地服务器的远端tcp客户端忽略
                        if (IPPROTO_UDP == pstcbInput->ubIPProto || (IPPROTO_TCP == pstcbInput->ubIPProto && TCP_TYPE_RCLIENT != pstcbInput->uniHandle.stTcpUdp.bType))
                        {
                            nInput = pstNextNode->uniData.nVal;                            

                            if (sizeof(pvAttach) == 4)
                            {
                                if (IPPROTO_TCP == pstcbInput->ubIPProto && TCP_TYPE_SERVER == pstcbInput->uniHandle.stTcpUdp.bType)
                                    *((UINT *)pvAttach) = (UINT)0; 
                                else
                                    *((UINT *)pvAttach) = (UINT)pstcbInput->pvAttach;
                            }
                            else
                            {
                                if (IPPROTO_TCP == pstcbInput->ubIPProto && TCP_TYPE_SERVER == pstcbInput->uniHandle.stTcpUdp.bType)
                                    *((ULONGLONG *)pvAttach) = (ULONGLONG)0; 
                                else
                                    *((ULONGLONG *)pvAttach) = (ULONGLONG)pstcbInput->pvAttach;
                            }
                            break;
                        }                        
                    }
                }

                /*
                if (unNetifIp == pstcbInput->uniHandle.stAddr.unNetifIp && usPort == pstcbInput->uniHandle.stAddr.usPort)
                {
                    nInput = pstNextNode->uniData.nVal;
                    if (sizeof(pvAttach) == 4)
                        *((UINT *)pvAttach) = (UINT)pstcbInput->pvAttach;
                    else
                        *((ULONGLONG *)pvAttach) = (ULONGLONG)pstcbInput->pvAttach;
                    break;
                }
                */
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return nInput;
}

#if SUPPORT_SACK
INT onps_tcp_send(INT nInput, UCHAR *pubData, INT nDataLen)
{
    if (nInput < 0 || nInput > SOCKET_NUM_MAX - 1)
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
        return -1;
    }

    if (!pubData || !nDataLen)
        return 0;

    os_critical_init();
    
    UCHAR ubErr; 
    PSTCB_ONPS_INPUT pstcbInput = &l_stcbaInput[nInput]; 
    if (IPPROTO_TCP == (EN_IPPROTO)pstcbInput->ubIPProto)
    {
        INT nCpyBytes;
        if (pstcbInput->pvAttach)
        {
            PST_TCPLINK pstLink = (PST_TCPLINK)pstcbInput->pvAttach;
            if (pstLink->bState == TLSCONNECTED)
            {
                os_enter_critical();
                {
                    UINT unAckIdx = (pstLink->stLocal.unSeqNum - 1) % TCPSNDBUF_SIZE;
                    UINT unWriteIdx = pstLink->stcbSend.unWriteBytes % TCPSNDBUF_SIZE;
                    if (unWriteIdx >= unAckIdx)
                    {
                        if (!(unWriteIdx == unAckIdx && pstLink->stcbSend.unWriteBytes != pstLink->stLocal.unSeqNum - 1))
                        {
                            UINT unRemainBytes = TCPSNDBUF_SIZE - unWriteIdx;
                            if (nDataLen <= (INT)unRemainBytes)
                            {
                                nCpyBytes = nDataLen;
                                memcpy(pstLink->stcbSend.pubSndBuf + unWriteIdx, pubData, nCpyBytes);
                                pstLink->stcbSend.unWriteBytes += (UINT)nCpyBytes;
                            }
                            else
                            {
                                //* 复制到剩余空间
                                nCpyBytes = (INT)unRemainBytes;
                                memcpy(pstLink->stcbSend.pubSndBuf + unWriteIdx, pubData, nCpyBytes);

                                //* 计算还剩下多少字节的数据然后把这些数据放到缓冲区的头部
                                INT nDataRemainBytes = nDataLen - nCpyBytes;
                                nDataRemainBytes = nDataRemainBytes > unAckIdx ? unAckIdx : nDataRemainBytes;
                                memcpy(pstLink->stcbSend.pubSndBuf, pubData + nCpyBytes, nDataRemainBytes);
                                nCpyBytes += nDataRemainBytes;
                                pstLink->stcbSend.unWriteBytes += (UINT)nCpyBytes;
                            }
                        }
                        else //* 读写指针相等，但读写字节数不相等意味着写指针追上读指针，缓冲区已满
                            nCpyBytes = 0;
                    }
                    else
                    {
                        nCpyBytes = nDataLen <= (unAckIdx - unWriteIdx) ? nDataLen : (unAckIdx - unWriteIdx);
                        memcpy(pstLink->stcbSend.pubSndBuf + unWriteIdx, pubData, nCpyBytes);
                        pstLink->stcbSend.unWriteBytes += (UINT)nCpyBytes;
                    }
                }
                os_exit_critical();                 

                //if (nCpyBytes)
                //{
                    tcp_link_for_send_data_put(pstLink);    //* 添加到数据发送队列
                    tcp_send_sem_post();                    //* 投递信号到完成实际tcp发送的线程
                //}                    

                return nCpyBytes;
            }
            else            
                ubErr = (UCHAR)ERRTCPNOTCONNECTED;             
        }
        else
            ubErr = (UCHAR)ERRTCPLINKCBNULL; 
    }
    else
        ubErr = (UCHAR)ERRIPROTOMATCH;

    os_enter_critical();
    pstcbInput->ubLastErr = ubErr;
    os_exit_critical();

    return -1; 
}
#endif


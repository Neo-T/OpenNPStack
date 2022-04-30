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

//* 协议栈icmp/tcp/udp层接收处理节点
typedef struct _STCB_ONPS_INPUT_ {
    HSEM hSem;          //* 信号量，一旦收到数据，协议栈将投递信号量给上层接收者，避免上层调用者采用轮询读取数据的方式，避免CPU资源被过多占用
    UCHAR ubIPProto;    //* IP上层协议定义，目前支持icmp/tcp/udp接收，其值来自于ip_frame.h中EN_IPPROTO枚举定义

    UCHAR ubLastErr;    //* 最近的错误信息，实际类型为EN_ONPSERR

    union {  //* 系统分配的接收者句柄，根据不同的上层协议其句柄信息均有所不同
        struct {
            USHORT usIdentifier;
        } stIcmp; //* icmp层句柄

        struct {
            USHORT usPort;
            UINT unIP;             
        } stTcp; //* tcp层句柄，使用IP地址和端口就可以唯一的标识一个tcp连接

        struct {
            USHORT usPort;
            UINT unIP;            
        } stUdp; //* udp层句柄，同tcp
    } uniHandle;
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

    l_hMtxInput = os_thread_mutex_init();
    if (INVALID_HMUTEX == l_hMtxInput)
    {
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
        pstcbInput->ubIPProto = (UCHAR)enProtocol; 
        pstcbInput->pubRcvBuf = pubRcvBuf; 
        pstcbInput->unRcvBufSize = unSize;

        if (IPPROTO_ICMP == enProtocol)
            pstcbInput->uniHandle.stIcmp.usIdentifier = 0;
        else if (IPPROTO_TCP == enProtocol)
        {
            pstcbInput->uniHandle.stTcp.unIP = 0;
            pstcbInput->uniHandle.stTcp.usPort = 0;             
            pstcbInput->pvAttach = NULL; 
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

    PSTCB_ONPS_INPUT pstcbInput = &l_stcbaInput[nInput]; 

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

    //* 归还节点
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList; 
        while (pstNextNode)
        {
            if (pstNextNode->uniData.nIndex == nInput)
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

static BOOL onps_input_set_recv_buf_size(PSTCB_ONPS_INPUT pstcbInput, UINT unRcvBufSize, EN_ONPSERR *penErr)
{
    if (pstcbInput->unRcvBufSize == unRcvBufSize)
        return TRUE; 

    if (pstcbInput->pubRcvBuf)
    {
        buddy_free(pstcbInput->pubRcvBuf);
        pstcbInput->unRcvBufSize = 0; 
    }

    pstcbInput->pubRcvBuf = (UCHAR *)buddy_alloc(unRcvBufSize, penErr);
    if (NULL == pstcbInput->pubRcvBuf)    
        return FALSE;     
    pstcbInput->unRcvBufSize = unRcvBufSize; 
    return TRUE; 
}

BOOL onps_input_set(INT nInput, ONPSIOPT enInputOpt, const void *pvVal, EN_ONPSERR *penErr)
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
    case IOPT_RCVBUFSIZE:
        return onps_input_set_recv_buf_size(pstcbInput, *((UINT*)pvVal), penErr);  

    case IOPT_SETICMPECHOID:
        if (pstcbInput->ubIPProto == IPPROTO_ICMP)
            pstcbInput->uniHandle.stIcmp.usIdentifier = *((USHORT *)pvVal); 
        else
        {
            if (penErr)
                *penErr = ERRIPROTOMATCH; 
            return FALSE; 
        }
        break;

    case IOPT_SETIP:
        if (pstcbInput->ubIPProto == IPPROTO_TCP)        
            pstcbInput->uniHandle.stTcp.unIP = *((UINT *)pvVal); 
        else if(pstcbInput->ubIPProto == IPPROTO_UDP)            
            pstcbInput->uniHandle.stUdp.unIP = *((UINT *)pvVal);
        else
        {
            if (penErr)
                *penErr = ERRIPROTOMATCH;
            return FALSE;
        }
        break; 

    case IOPT_SETPORT: 
        if (pstcbInput->ubIPProto == IPPROTO_TCP)
            pstcbInput->uniHandle.stTcp.usPort = *((USHORT *)pvVal);
        else if (pstcbInput->ubIPProto == IPPROTO_UDP)
            pstcbInput->uniHandle.stUdp.usPort = *((USHORT *)pvVal);
        else
        {
            if (penErr)
                *penErr = ERRIPROTOMATCH;
            return FALSE;
        }
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
        break; 

    default:
        if (penErr)
            *penErr = ERRUNSUPPIOPT; 
        return FALSE;
    }

    return TRUE; 
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
        *((ULONGLONG *)pvVal) = (ULONGLONG)pstcbInput->pvAttach;
        break; 

    default:
        if (penErr)
            *penErr = ERRUNSUPPIOPT;
        return FALSE;
    }

    return TRUE;
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
            if (pstcbInput->ubIPProto == IPPROTO_ICMP && usIdentifier == pstcbInput->uniHandle.stIcmp.usIdentifier)
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

UCHAR *onps_input_get_rcv_buf(INT nInput, HSEM *phSem, UINT *punRcvedBytes)
{
    if (nInput > SOCKET_NUM_MAX - 1)
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("onps_input_get_rcv_buf() failed, Handle %d is out of system scope\r\n", nInput);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return NULL;
    }

    if (phSem)
        *phSem = l_stcbaInput[nInput].hSem; 

    l_stcbaInput[nInput].unRcvedBytes = *punRcvedBytes < l_stcbaInput[nInput].unRcvBufSize ? *punRcvedBytes : l_stcbaInput[nInput].unRcvBufSize;

    return l_stcbaInput[nInput].pubRcvBuf; 
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
    if (os_thread_sem_pend(l_stcbaInput[nInput].hSem, nWaitSecs) < 0)
        return 0; 

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
#if SUPPORT_PRINTF
#if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
#endif
        printf("onps_get_last_error() failed, Handle %d is out of system scope\r\n", nInput);
#if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
#endif
#endif
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

BOOL onps_input_tcp_port_used(USHORT usPort)
{
    BOOL blIsUsed = FALSE;
    os_thread_mutex_lock(l_hMtxInput);
    {
        PST_SLINKEDLIST_NODE pstNextNode = l_pstInputSLList;
        PSTCB_ONPS_INPUT pstcbInput;
        while (pstNextNode)
        {
            pstcbInput = &l_stcbaInput[pstNextNode->uniData.nIndex];
            if (pstcbInput->ubIPProto == IPPROTO_TCP)
            {
                if (usPort == pstcbInput->uniHandle.stTcp.usPort)
                {
                    blIsUsed = TRUE; 
                    break; 
                }
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxInput);

    return blIsUsed; 
}

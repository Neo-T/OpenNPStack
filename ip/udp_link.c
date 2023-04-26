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
#include "onps_input.h"
#define SYMBOL_GLOBALS
#include "ip/udp_link.h"
#undef SYMBOL_GLOBALS

static ST_UDPLINK l_staUdpLinkNode[UDP_LINK_NUM_MAX]; 
static PST_UDPLINK l_pstFreeUdpLinkList = NULL;
static HMUTEX l_hMtxUdpLinkList = INVALID_HMUTEX;

BOOL udp_link_init(EN_ONPSERR *penErr)
{
    //* 链接	
    INT i;
    for (i = 0; i < TCP_LINK_NUM_MAX - 1; i++)
    {
        l_staUdpLinkNode[i].bIdx = i; 
        l_staUdpLinkNode[i].bNext = i + 1;
    }
    l_staUdpLinkNode[i].bIdx = i; 
    l_staUdpLinkNode[i].bNext = -1; 
    l_pstFreeUdpLinkList = &l_staUdpLinkNode[0]; 

    l_hMtxUdpLinkList = os_thread_mutex_init();
    if (INVALID_HMUTEX != l_hMtxUdpLinkList)
        return TRUE;

    if (penErr)
        *penErr = ERRMUTEXINITFAILED;
    return FALSE;
}

void udp_link_uninit(void)
{
    if (INVALID_HMUTEX != l_hMtxUdpLinkList)
        os_thread_mutex_uninit(l_hMtxUdpLinkList);
}

PST_UDPLINK udp_link_get(EN_ONPSERR *penErr)
{
    PST_UDPLINK pstFreeNode;
    os_thread_mutex_lock(l_hMtxUdpLinkList);
    {
        if (NULL == l_pstFreeUdpLinkList)
        {
            os_thread_mutex_unlock(l_hMtxUdpLinkList);

            if (penErr)
                *penErr = ERRNOTCPLINKNODE;

            return NULL;
        }

        pstFreeNode = l_pstFreeUdpLinkList;
        if (l_pstFreeUdpLinkList->bNext >= 0)
            l_pstFreeUdpLinkList = &l_staUdpLinkNode[l_pstFreeUdpLinkList->bNext]; 
        else        
            l_pstFreeUdpLinkList = NULL;
    }
    os_thread_mutex_unlock(l_hMtxUdpLinkList);
    
    pstFreeNode->stPeerAddr.saddr_ipv4 = 0;
    pstFreeNode->stPeerAddr.usPort = 0; 
    return pstFreeNode;
}

void udp_link_free(PST_UDPLINK pstTcpLink)
{    
    os_thread_mutex_lock(l_hMtxUdpLinkList);
    {
        if (l_pstFreeUdpLinkList)        
            pstTcpLink->bNext = l_pstFreeUdpLinkList->bIdx;         
        else        
            pstTcpLink->bNext = -1; 
        l_pstFreeUdpLinkList = pstTcpLink;
    }
    os_thread_mutex_unlock(l_hMtxUdpLinkList);
}

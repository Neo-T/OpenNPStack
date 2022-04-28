#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "onps_input.h"
#define SYMBOL_GLOBALS
#include "ip/tcp_link.h"
#undef SYMBOL_GLOBALS

static ST_TCPLINK l_staTcpLink[TCP_LINK_NUM_MAX]; 
static SHORT l_saTcpLinkNode[TCP_LINK_NUM_MAX];
static SHORT l_sFreeTcpLinkList = -1; 
static HMUTEX l_hMtxTcpLinkList = INVALID_HMUTEX;

BOOL tcp_link_init(EN_ONPSERR *penErr)
{
    //* 链接	
    INT i;
    for (i = 0; i < TCP_LINK_NUM_MAX - 1; i++)
        l_saTcpLinkNode[i] = i + 1;
    l_saTcpLinkNode[TCP_LINK_NUM_MAX - 1] = -1;
    l_sFreeTcpLinkList = 0;

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

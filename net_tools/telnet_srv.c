/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "telnet/os_nvt.h"

#if NETTOOLS_TELNETSRV
#define SYMBOL_GLOBALS
#include "net_tools/telnet_srv.h"
#undef SYMBOL_GLOBALS

#include "net_tools/telnet.h"
#include "telnet/nvt_cmd.h"

static PSTCB_TELNETCLT l_pstcbTelnetCltList = NULL; 
static CHAR l_bTelnetSrvState = 1; 

//* 以任务/线程方式启动网络虚拟终端（NVT）
BOOL nvt_start(PSTCB_TELNETCLT pstcbTelnetClt)
{    
    return os_nvt_start(&pstcbTelnetClt->stcbNvt);
}

//* 停止网络虚拟终端（NVT）
void nvt_stop(PSTCB_TELNETCLT pstcbTelnetClt)
{
    pstcbTelnetClt->bitTHRunEn = FALSE; 

    UINT unTimeCounts = 0; 
    while (!pstcbTelnetClt->bitTHIsEnd && unTimeCounts++ < (3 * 100)) 
        os_sleep_ms(10); 
    
    //* 如果还未结束则强制当前nvt结束   
    if(!pstcbTelnetClt->bitTHIsEnd)
        os_nvt_stop(&pstcbTelnetClt->stcbNvt);
}

static void telnet_client_add(PSTCB_TELNETCLT pstcbTelnetClt)
{
    pstcbTelnetClt->pstcbNext = l_pstcbTelnetCltList;
    l_pstcbTelnetCltList = pstcbTelnetClt;
}

static void telnet_client_del(PSTCB_TELNETCLT pstcbTelnetClt)
{
    PSTCB_TELNETCLT pstcbNextNode = l_pstcbTelnetCltList;
    PSTCB_TELNETCLT pstcbPrevNode = NULL;
    while (pstcbNextNode)
    {
        //* 找到要摘除的节点并摘除之
        if (pstcbNextNode == pstcbTelnetClt)
        {
            if (pstcbPrevNode)
                pstcbPrevNode->pstcbNext = pstcbTelnetClt->pstcbNext;
            else
                l_pstcbTelnetCltList = pstcbTelnetClt->pstcbNext;

            break;
        }

        pstcbPrevNode = pstcbNextNode;
        pstcbNextNode = pstcbNextNode->pstcbNext;
    }
}

PSTCB_TELNETCLT telnet_client_next(PSTCB_TELNETCLT *ppstcbNextClt)
{
    if (l_pstcbTelnetCltList)
    {
        PSTCB_TELNETCLT pstcbNextNode;
        if (*ppstcbNextClt)
            pstcbNextNode = *ppstcbNextClt;
        else
            pstcbNextNode = l_pstcbTelnetCltList;

        *ppstcbNextClt = pstcbNextNode->pstcbNext;
        return pstcbNextNode;
    }
    else
        return NULL;
}

static BOOL telnet_client_new(SOCKET hCltSocket)
{
    EN_ONPSERR enErr;
    
    //* 非阻塞
    socket_set_rcv_timeout(hCltSocket, 0, &enErr); 

    PSTCB_TELNETCLT pstcbClt = (PSTCB_TELNETCLT)buddy_alloc(sizeof(STCB_TELNETCLT), &enErr)/*telnet_client_node_get(NULL, &enErr)*/;
    if (pstcbClt)
    {
        pstcbClt->hClient = hCltSocket;
        pstcbClt->unLastOperateTime = os_get_system_secs() - 5;
        pstcbClt->bitTHRunEn = TRUE;
    }
    else
        goto __lblErr;

    if (nvt_start(pstcbClt))
    {
        telnet_client_add(pstcbClt); 
        pstcbClt->bitTHIsEnd = FALSE; 
    }
    else
    {
        enErr = ERRNVTSTART;
        goto __lblErr;
    }

    return TRUE; 

__lblErr:
    if (pstcbClt)
        buddy_free(pstcbClt)/*telnet_client_node_free(pstcbClt)*/;


    //* 给客户端下发一条错误信息
    send(hCltSocket, "Failed to create a new telnet client, ", sizeof("Failed to create a new telnet client, ") - 1, 1);
    const CHAR *pszErr = onps_error(enErr);
    send(hCltSocket, (UCHAR *)pszErr, strlen(pszErr), 1);
    os_sleep_secs(1);
    close(hCltSocket); 

    return FALSE; 
}

static void telnet_client_close(PSTCB_TELNETCLT pstcbTelnetClt)
{
    nvt_stop(pstcbTelnetClt);
    close(pstcbTelnetClt->hClient);
    telnet_client_del(pstcbTelnetClt);
    buddy_free(pstcbTelnetClt);
}

static void telnet_client_clean_zombie(CHAR *pbClientCnt)
{
    PSTCB_TELNETCLT pstcbNextNode = NULL;

    do {
        PSTCB_TELNETCLT pstcbNextClt = telnet_client_next(&pstcbNextNode);
        if (pstcbNextClt && ((os_get_system_secs() - pstcbNextClt->unLastOperateTime > TELNETCLT_INACTIVE_TIMEOUT) || pstcbNextClt->bitTHIsEnd))         
        {
            telnet_client_close(pstcbNextClt);
            *pbClientCnt -= 1; 
        }        
    } while (pstcbNextNode);
}

static void telnet_client_clean(void)
{
    PSTCB_TELNETCLT pstcbNextNode = NULL;

    do {
        PSTCB_TELNETCLT pstcbNextClt = telnet_client_next(&pstcbNextNode);
        if (pstcbNextClt)
            telnet_client_close(pstcbNextClt);
        else
            break;
    } while (pstcbNextNode);    
}

void telnet_srv_entry(void *pvParam)
{
    EN_ONPSERR enErr;     
    SOCKET hSrvSocket = tcpsrv_start(AF_INET, TELNETSRV_PORT, NVTNUM_MAX, TCPSRVRCVMODE_ACTIVE, &enErr); 

#if SUPPORT_IPV6 && TELNETSRV_SUPPORT_IPv6
    if (INVALID_SOCKET == hSrvSocket)
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif	
        printf("[4] telnet_srv_start() failed, %s\r\n", onps_error(enErr));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }

    SOCKET hSrvSocket6 = tcpsrv_start(AF_INET6, TELNETSRV_PORT, NVTNUM_MAX, TCPSRVRCVMODE_ACTIVE, &enErr); 
    if (INVALID_SOCKET == hSrvSocket6)
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif	
        printf("[6] telnet_srv_start() failed, %s\r\n", onps_error(enErr));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }
#endif //* #if SUPPORT_IPV6 && TELNETSRV_SUPPORT_IPv6 

#if SUPPORT_IPV6 && TELNETSRV_SUPPORT_IPv6
    if (INVALID_SOCKET != hSrvSocket || INVALID_SOCKET != hSrvSocket6)
#else
    if (INVALID_SOCKET != hSrvSocket)
#endif //* #if SUPPORT_IPV6 && TELNETSRV_SUPPORT_IPv6
    {
        //* 与操作系统相关的nvt初始化，其实就是完成nvt作为线程/任务启动前的准备工作
        os_nvt_init();

        //* 注册用户自定义的nvt指令
        nvt_cmd_register(); 

        //* 加载nvt内嵌指令
        nvt_embedded_cmd_loader(); 

        CHAR bClientCnt = 0; 
        SOCKET hClient = INVALID_SOCKET;
        while (l_bTelnetSrvState)
        {            
    #if SUPPORT_IPV6 && TELNETSRV_SUPPORT_IPv6
            if (INVALID_SOCKET != hSrvSocket)                           
                hClient = accept(hSrvSocket, NULL, NULL, 1, &enErr);            
            if (INVALID_SOCKET == hClient && INVALID_SOCKET != hSrvSocket6)
                hClient = accept(hSrvSocket6, NULL, NULL, 1, &enErr); 
    #else
            hClient = accept(hSrvSocket, NULL, NULL, 1, &enErr);
    #endif
            if (INVALID_SOCKET != hClient)
            {
                if (bClientCnt < NVTNUM_MAX)
                {
                    if(telnet_client_new(hClient))
                        bClientCnt++; 
                }
                else
                {
                    send(hClient, "The maximum number of logged in users has been reached, please try again later.\r\n", 
                        sizeof("The maximum number of logged in users has been reached, please try again later.\r\n") - 1, 1); 
                    close(hClient);
                }
            }        

            telnet_client_clean_zombie(&bClientCnt);
        }

#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif	
        printf("Telnet server terminated.\r\n"); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif

        telnet_client_clean();
    #if SUPPORT_IPV6 && TELNETSRV_SUPPORT_IPv6
        if (INVALID_SOCKET != hSrvSocket)
            close(hSrvSocket); 
        if (INVALID_SOCKET != hSrvSocket6)
            close(hSrvSocket6);
    #else
        close(hSrvSocket);
    #endif
        os_nvt_uninit(); 
    }
    else
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif	
        printf("telnet_srv_start() failed, %s\r\n", onps_error(enErr)); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }

    l_bTelnetSrvState = -1;
}

BOOL telnet_srv_end(void)
{
    if(1 == l_bTelnetSrvState)
        l_bTelnetSrvState = 0; 

    UINT unTimeCounts = 0;
    while (!l_bTelnetSrvState && unTimeCounts++ < (3 * 100))
        os_sleep_ms(10); 

    return (BOOL)(l_bTelnetSrvState < 0); 
}

#endif

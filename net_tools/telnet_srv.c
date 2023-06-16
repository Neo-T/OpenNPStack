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

#if NETTOOLS_TELNETSRV
#include "net_tools/telnet.h"
#define SYMBOL_GLOBALS
#include "net_tools/telnet_srv.h"
#undef SYMBOL_GLOBALS

static PSTCB_TELNETCLT l_pstcbTelnetCltList = NULL; 
static CHAR l_bTelnetSrvState = 1; 

//* 以任务/线程方式启动网络虚拟终端（NVT）
BOOL nvt_start(PSTCB_TELNETCLT pstcbTelnetClt)
{
    
    return TRUE;
}

//* 停止网络虚拟终端（NVT）
void nvt_stop(PSTCB_TELNETCLT pstcbTelnetClt)
{
    pstcbTelnetClt->bTHIsRunning = FALSE;
    
    //* 等待任务/线程安全结束
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

static void telnet_client_new(SOCKET hCltSocket)
{
    EN_ONPSERR enErr;
    PSTCB_TELNETCLT pstcbClt = (PSTCB_TELNETCLT)buddy_alloc(sizeof(STCB_TELNETCLT), &enErr)/*telnet_client_node_get(NULL, &enErr)*/;
    if (pstcbClt)
    {
        pstcbClt->hClient = hCltSocket;
        pstcbClt->unLastOperateTime = os_get_system_secs() - 5;
        pstcbClt->bTHIsRunning = TRUE;
    }
    else
        goto __lblErr;

    if (nvt_start(pstcbClt))
        telnet_client_add(pstcbClt);
    else
    {
        enErr = ERRNVTSTART;
        goto __lblErr;
    }

    return;

__lblErr:
    if (pstcbClt)
        buddy_free(pstcbClt)/*telnet_client_node_free(pstcbClt)*/;


    //* 给客户端下发一条错误信息
    send(hCltSocket, "Failed to create a new telnet client, ", sizeof("Failed to create a new telnet client, ") - 1, 0);
    const CHAR *pszErr = onps_error(enErr);
    send(hCltSocket, pszErr, strlen(pszErr), 0);

    return;
}

static void telnet_client_close(PSTCB_TELNETCLT pstcbTelnetClt)
{
    nvt_stop(pstcbTelnetClt);
    closesocket(pstcbTelnetClt->hClient);
    telnet_client_del(pstcbTelnetClt);
    buddy_free(pstcbTelnetClt);
}

static void telnet_client_clean_zombie(CHAR *pbClientCnt)
{
    PSTCB_TELNETCLT pstcbNextNode = NULL;

    do {
        PSTCB_TELNETCLT pstcbNextClt = telnet_client_next(&pstcbNextNode);
        if (pstcbNextClt && os_get_system_secs() - pstcbNextClt->unLastOperateTime > TELNETCLT_INACTIVE_TIMEOUT)
        {
            telnet_client_close(pstcbNextClt);
            *pbClientCnt -= 1; 
        }
        else
            break;
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

void telnet_srv_entry(void)
{
    EN_ONPSERR enErr;     
    SOCKET hSrvSocket = tcp_srv_start(AF_INET, TELNETSRV_PORT, NVTNUM_MAX, &enErr); 
    if (INVALID_SOCKET != hSrvSocket)
    {
        CHAR bClientCnt = 0; 
        while (l_bTelnetSrvState)
        {
            SOCKET hClient = accept(hSrvSocket, NULL, NULL, 1, &enErr);
            if (INVALID_SOCKET != hClient)
            {
                if (bClientCnt < NVTNUM_MAX)
                {
                    telnet_client_new(hClient);
                    bClientCnt++;
                }
                else
                {
                    send(hClient, "The maximum number of logged in users has been reached, please try again later.\r\n", sizeof("The maximum number of logged in users has been reached, please try again later.\r\n") - 1, 1); 
                    close(hClient);
                }
            }
            else
            {
        #if SUPPORT_PRINTF
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf);
            #endif	
                printf("accept() failed in file %s, %s\r\n", __FILE__, onps_error(enErr)); 
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf);
            #endif
        #endif
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
        close(hSrvSocket);
        l_bTelnetSrvState = -1; 
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
}

void telnet_srv_end(void)
{
    l_bTelnetSrvState = 0; 
    while (!l_bTelnetSrvState)
        os_sleep_ms(10); 
}

#endif

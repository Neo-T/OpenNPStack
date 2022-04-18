#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"

#define SYMBOL_GLOBALS
#include "netif/route.h"
#undef SYMBOL_GLOBALS

static ST_ROUTE_NODE l_staRouteNode[NETIF_NUM];
static PST_ROUTE_NODE l_pstFreeNode = NULL;
static PST_ROUTE_NODE l_pstRouteLink = NULL;
static HMUTEX l_hMtxRoute = INVALID_HMUTEX;
BOOL route_table_init(EN_ONPSERR *penErr)
{
    //* 路由表清零
    memset(&l_staRouteNode[0], 0, sizeof(l_staRouteNode));

    //* 初始化
    INT i;
    for (i = 1; i < NETIF_NUM; i++)
        l_staRouteNode[i - 1].pstNext = &l_staRouteNode[i];
    l_pstFreeNode = &l_staRouteNode[0];

    l_hMtxRoute = os_thread_mutex_init();
    if (INVALID_HMUTEX != l_hMtxRoute)
        return TRUE;

    if (penErr)
        *penErr = ERRMUTEXINITFAILED;

    return FALSE;
}

void route_table_uninit(void)
{
    l_pstRouteLink = NULL;
    l_pstFreeNode = NULL;

    if (INVALID_HMUTEX != l_hMtxRoute)
        os_thread_mutex_uninit(l_hMtxRoute);
}

static PST_ROUTE_NODE get_free_node(void)
{
    PST_ROUTE_NODE pstNode;
    os_thread_mutex_lock(l_hMtxRoute);
    {
        pstNode = l_pstFreeNode;
        if (l_pstFreeNode)
            l_pstFreeNode = l_pstFreeNode->pstNext;
    }
    os_thread_mutex_unlock(l_hMtxRoute);

    memset(&pstNode->stRoute, 0, sizeof(pstNode->stRoute));
    return pstNode;
}

static void put_free_node(PST_ROUTE_NODE pstNode)
{
    os_thread_mutex_lock(l_hMtxRoute);
    {
        pstNode->pstNext = l_pstFreeNode;
        l_pstFreeNode = pstNode;
    }
    os_thread_mutex_unlock(l_hMtxRoute);
}

BOOL route_add(PST_NETIF pstNetif, UINT unDestination, UINT unGateway, UINT unGenmask, EN_ONPSERR *penErr)
{
    PST_ROUTE_NODE pstNode; 

#if SUPPORT_PRINTF
    UCHAR *pubAddr;
#endif

    //* 先看看是否已经存在这个目标网段
    os_thread_mutex_lock(l_hMtxRoute);
    {
        PST_ROUTE_NODE pstNextNode = l_pstRouteLink;
        while (pstNextNode)
        {
            if (unDestination == pstNextNode->stRoute.unDestination) //* 目标网段相等
            {
                pstNextNode->stRoute.unGateway = unGateway;
                pstNextNode->stRoute.unGenmask = unGenmask;
                pstNextNode->stRoute.pstNetif = pstNetif; 
                pstNode = pstNextNode;
                goto __lblEnd;
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxRoute);

    //* 执行到这里意味着没找到对应的路由条目，需要新增一个
    pstNode = get_free_node();
    if (NULL == pstNode)
    {
        if (penErr)
            *penErr = ERRNOROUTENODE;

        return FALSE;
    }

    //* 保存路由相关信息
    pstNode->stRoute.unDestination = unDestination; 
    pstNode->stRoute.unGateway = unGateway; 
    pstNode->stRoute.unGenmask = unGenmask; 
    pstNode->stRoute.pstNetif = pstNetif; 

    //* 加入链表
    os_thread_mutex_lock(l_hMtxRoute);
    {
        pstNode->pstNext = l_pstRouteLink;
        l_pstRouteLink = pstNode;
    }
    os_thread_mutex_unlock(l_hMtxRoute);

__lblEnd: 
#if SUPPORT_PRINTF
    pubAddr = (UCHAR *)&pstNode->stRoute.unDestination;
    printf("Add network interface <%s> to routing table\r\n", pstNode->stRoute.pstNetif->szName); 
    if (pstNode->stRoute.unDestination)
    {
        printf("destination %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    }
    else
        printf("destination default");
    pubAddr = (UCHAR *)&pstNode->stRoute.unGateway;
    printf(", gateway %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);    
    pubAddr = (UCHAR *)&pstNode->stRoute.unGenmask;
    printf(", genmask %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]); 
#endif

    return TRUE;
}

void route_del(UINT unDestination)
{
    PST_ROUTE_NODE pstNode = NULL;

    //* 从路由表删除
    os_thread_mutex_lock(l_hMtxRoute);
    {
        PST_ROUTE_NODE pstNextNode = l_pstRouteLink;
        PST_ROUTE_NODE pstPrevNode = NULL;
        while (pstNextNode)
        {
            if (pstNextNode->stRoute.unDestination == unDestination)
            {
                if (pstPrevNode)
                    pstPrevNode->pstNext = pstNextNode->pstNext;
                else
                    l_pstRouteLink = pstNextNode->pstNext;
                pstNode = pstNextNode;
                break;
            }
            pstPrevNode = pstNextNode;
            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxRoute);

    if(pstNode)
        put_free_node(pstNode);
}

PST_NETIF route_get_netif(UINT unDestination)
{
    PST_NETIF pstNetif = NULL; 
    PST_NETIF pstDefaultNetif = NULL;

    os_thread_mutex_lock(l_hMtxRoute);
    {
        PST_ROUTE_NODE pstNextNode = l_pstRouteLink;
        while (pstNextNode)
        { 
            if (pstNextNode->stRoute.unDestination)
            {
                if (ip_addressing(unDestination, pstNextNode->stRoute.unDestination, pstNextNode->stRoute.unGenmask))
                {
                    pstNetif = pstNextNode->stRoute.pstNetif; 
                    break;
                }
            }
            else
                pstDefaultNetif = pstNextNode->stRoute.pstNetif;             

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxRoute);

    if (pstNetif)
        return pstNetif;
    else
        return pstDefaultNetif; 
}

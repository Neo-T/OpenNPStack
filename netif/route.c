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
                pstNextNode->stRoute.pstNetif->bUsedCount = 0; 
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
    pstNode->stRoute.pstNetif->bUsedCount = 0;

__lblEnd: 
#if SUPPORT_PRINTF
    pubAddr = (UCHAR *)&pstNode->stRoute.unDestination;
#if PRINTF_THREAD_MUTEX
    os_thread_mutex_lock(o_hMtxPrintf);
#endif
    printf("Add network interface <%s> to routing table\r\n[", pstNode->stRoute.pstNetif->szName); 
    if (pstNode->stRoute.unDestination)
    {
        printf("destination %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    }
    else
        printf("destination default");
    pubAddr = (UCHAR *)&pstNode->stRoute.unGateway;
    printf(", gateway %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);    
    pubAddr = (UCHAR *)&pstNode->stRoute.unGenmask;
    printf(", genmask %d.%d.%d.%d]\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]); 
#if PRINTF_THREAD_MUTEX
    os_thread_mutex_unlock(o_hMtxPrintf);
#endif
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

void route_del_ext(PST_NETIF pstNetif)
{
    PST_ROUTE_NODE pstNode = NULL;

    //* 从路由表删除
    os_thread_mutex_lock(l_hMtxRoute);
    {
        //* 等待网络接口使用计数清零，只有清零才可删除当前网络接口在路由表中的所有条目
        while (pstNetif->bUsedCount)
            os_sleep_ms(10);

        PST_ROUTE_NODE pstNextNode = l_pstRouteLink;
        PST_ROUTE_NODE pstPrevNode = NULL;
        while (pstNextNode)
        {
            if (pstNextNode->stRoute.pstNetif == pstNetif)
            {
                if (pstPrevNode)                
                    pstPrevNode->pstNext = pstNextNode->pstNext;                                    
                else
                    l_pstRouteLink = pstNextNode->pstNext;
                pstNode = pstNextNode->pstNext;
                 
                //* 归还节点
                pstNextNode->pstNext = l_pstFreeNode;
                l_pstFreeNode = pstNextNode;

                //* 继续查找，因为路由表中同一个网络接口可能存在一个以上的路由条目
                pstNextNode = pstNode; 
            }
            else
            {
                pstPrevNode = pstNextNode;
                pstNextNode = pstNextNode->pstNext;
            }            
        }
    }
    os_thread_mutex_unlock(l_hMtxRoute);
}

PST_NETIF route_get_netif(UINT unDestination, BOOL blIsForSending, in_addr_t *punSrcIp)
{
    PST_NETIF pstNetif = NULL; 
    PST_NETIF pstDefaultNetif = NULL;

    //* 先查找ethernet网卡链表（PPP不需要，因为这个只能按照既定规则发到拨号网络的对端），看看本地网段是否就可以满足要求，而不是需要查找路由表
    pstNetif = netif_get_eth_by_genmask(unDestination, punSrcIp);
    if (pstNetif)
        return pstNetif; 

    //* 查找路由表
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

        //* 如果调用者调用该函数获取网络接口是用于发送，则需要对此进行计数，以确保使用期间该接口不会被删除
        if (blIsForSending)
        {
            if (pstNetif)
                pstNetif->bUsedCount++;
            else
            {
                if(pstDefaultNetif)
                    pstDefaultNetif->bUsedCount++; 
            }
        }        
    }
    os_thread_mutex_unlock(l_hMtxRoute);

    if (pstNetif)
    {
        if (punSrcIp)
            *punSrcIp = pstNetif->stIPv4.unAddr; 
        return pstNetif;
    }
    else
    {
        if (pstDefaultNetif)
        {
            if (punSrcIp)
                *punSrcIp = pstDefaultNetif->stIPv4.unAddr;
            return pstDefaultNetif;
        }
        else //* 缺省路由也为空，则直接使用网络接口链表的首节点作为缺省路由
        {
            pstNetif = netif_get_first(blIsForSending); 
            if (pstNetif)
            {
                if (punSrcIp)
                    *punSrcIp = pstNetif->stIPv4.unAddr;
            }

            return pstNetif; 
        }
    }
}

UINT route_get_netif_ip(UINT unDestination)
{
    UINT unNetifIp = 0; 

    //* 查找本地ethernet网卡链表，先看看是否目标地址在同一个网段
    PST_NETIF pstNetif = netif_get_eth_by_genmask(unDestination, &unNetifIp);
    if (pstNetif)    
        return unNetifIp;

    //* 本地网段不匹配，只能走路由了
    os_thread_mutex_lock(l_hMtxRoute);
    {
        PST_ROUTE_NODE pstNextNode = l_pstRouteLink;
        while (pstNextNode)
        {
            if (pstNextNode->stRoute.unDestination)
            {
                if (ip_addressing(unDestination, pstNextNode->stRoute.unDestination, pstNextNode->stRoute.unGenmask))
                {                    
                    unNetifIp = pstNextNode->stRoute.pstNetif->stIPv4.unAddr; 
                    break;
                }
            }
            else
            {
                unNetifIp = pstNextNode->stRoute.pstNetif->stIPv4.unAddr; 
            }

            pstNextNode = pstNextNode->pstNext;
        }          
    }
    os_thread_mutex_unlock(l_hMtxRoute);

    if (!unNetifIp)
        unNetifIp = netif_get_first_ip();

    return unNetifIp; 
}

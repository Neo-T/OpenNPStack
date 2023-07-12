/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"

#define SYMBOL_GLOBALS
#include "netif/route.h"
#undef SYMBOL_GLOBALS

static ST_ROUTE_NODE l_staRouteNode[ROUTE_ITEM_NUM];
static PST_ROUTE_NODE l_pstFreeNode = NULL;
static PST_ROUTE_NODE l_pstRouteLink = NULL;
static HMUTEX l_hMtxRoute = INVALID_HMUTEX;

BOOL route_table_init(EN_ONPSERR *penErr)
{
    //* 路由表清零
    memset(&l_staRouteNode[0], 0, sizeof(l_staRouteNode));

    //* 初始化
    INT i;
    for (i = 1; i < ROUTE_ITEM_NUM; i++)
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

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
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
                pstNextNode->stRoute.unSource = netif_get_source_ip_by_gateway(pstNetif, unGateway);
                pstNextNode->stRoute.unGateway = unGateway;
                pstNextNode->stRoute.unGenmask = unGenmask;
                pstNextNode->stRoute.pstNetif = pstNetif; 
                pstNextNode->stRoute.pstNetif->bUsedCount = 0; 
                pstNode = pstNextNode;
				os_thread_mutex_unlock(l_hMtxRoute); 
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
    pstNode->stRoute.unSource = netif_get_source_ip_by_gateway(pstNetif, unGateway); 
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
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    pubAddr = (UCHAR *)&pstNode->stRoute.unDestination;
#if PRINTF_THREAD_MUTEX
    os_thread_mutex_lock(o_hMtxPrintf);
#endif
    printf("Add/Update network interface <%s> to routing table\r\n[", pstNode->stRoute.pstNetif->szName); 
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

BOOL route_del(UINT unDestination, EN_ONPSERR *penErr)
{
    PST_ROUTE_NODE pstNode = NULL;

    //* 缺省路由不能删除，只能更新
    if (!unDestination)
    {
        if (penErr)
            *penErr = ERRROUTEDEFAULTDEL;
        return FALSE; 
    }

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

    if (pstNode)
    {
        put_free_node(pstNode);
        return TRUE; 
    }

    if (penErr)
        *penErr = ERRROUTEENTRYNOTEXIST; 
    return FALSE; 
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

PST_NETIF route_get_netif(UINT unDestination, BOOL blIsForSending, in_addr_t *punSrcIp, in_addr_t *punArpDstAddr)
{
    PST_ROUTE pstRoute = NULL/*PST_NETIF pstNetif = NULL*/;
    PST_ROUTE pstDefaultRoute = NULL/*PST_NETIF pstDefaultNetif = NULL*/;
    PST_NETIF pstNetif = NULL; 

#if SUPPORT_ETHERNET
    //* 先查找ethernet网卡链表（PPP链路不需要，因为这个只能按照既定规则发到拨号网络的对端），看看本地网段是否就可以满足要求，而不是需要查找路由表
    pstNetif = netif_eth_get_by_genmask(unDestination, punSrcIp, blIsForSending);
    if (pstNetif)
        return pstNetif; 
#endif

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
                    pstRoute/*pstNetif*/ = &pstNextNode->stRoute/*.pstNetif*/;
                    break;
                }
            }
            else
                pstDefaultRoute/*pstDefaultNetif*/ = &pstNextNode->stRoute/*.pstNetif*/;

            pstNextNode = pstNextNode->pstNext;
        }

		if (!pstRoute)
			pstRoute = pstDefaultRoute; 

        //* 如果调用者调用该函数获取网络接口是用于发送，则需要对此进行计数，以确保使用期间该接口不会被删除
        if (blIsForSending && pstRoute)
			pstRoute->pstNetif->bUsedCount++; 
    }
    os_thread_mutex_unlock(l_hMtxRoute);

    if (pstRoute)
    {
        if (punSrcIp)
            *punSrcIp = pstRoute->unSource/*pstNetif->stIPv4.unAddr*/;

        if (punArpDstAddr)
            *punArpDstAddr = pstRoute->unGateway/*pstNetif->stIPv4.unGateway*/;

		pstNetif = pstRoute->pstNetif;
    }
    
	return pstNetif; 
}

//* 获取缺省路由使用的网卡以获取其指定的dns服务器
PST_NETIF route_get_default(void)
{
    PST_ROUTE pstDefaultRoute = NULL; 

    //* 查找路由表
    os_thread_mutex_lock(l_hMtxRoute);
    {
        PST_ROUTE_NODE pstNextNode = l_pstRouteLink; 
        while (pstNextNode)
        {
            if (!pstNextNode->stRoute.unDestination)
            {
                pstDefaultRoute = &pstNextNode->stRoute;
                break; 
            }

            pstNextNode = pstNextNode->pstNext; 
        }
    }
    os_thread_mutex_unlock(l_hMtxRoute); 

    if (pstDefaultRoute)
        return pstDefaultRoute->pstNetif; 
    else //* 缺省路由为空，则直接使用网络接口链表的首节点作为缺省路由
        return netif_get_first(FALSE); 
}

UINT route_get_netif_ip(UINT unDestination)
{
    UINT unNetifIp = 0; 

#if SUPPORT_ETHERNET
    //* 查找本地ethernet网卡链表，先看看是否目标地址在同一个网段
    PST_NETIF pstNetif = netif_eth_get_by_genmask(unDestination, &unNetifIp, FALSE);
    if (pstNetif)    
        return unNetifIp;
#endif

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
                    unNetifIp = pstNextNode->stRoute.unSource; 
                    break;
                }
            }
            else
            {
                unNetifIp = pstNextNode->stRoute.unSource; 
            }

            pstNextNode = pstNextNode->pstNext;
        }          
    }
    os_thread_mutex_unlock(l_hMtxRoute);

    if (!unNetifIp)
        unNetifIp = netif_get_first_ip();

    return unNetifIp; 
}

#if SUPPORT_IPV6
PST_NETIF route_ipv6_get_netif(const UCHAR ubaDestination[16], BOOL blIsForSending, UCHAR *pubSource, UCHAR *pubNSAddr, UCHAR *pubHopLimit)
{
	PST_NETIF pstNetif = NULL;

#if SUPPORT_ETHERNET
	//* 先查找ethernet网卡链表（PPP链路不需要，因为这个只能按照既定规则发到拨号网络的对端），看看以太网是否就可以满足要求，否则就只能通过ppp链路转发了	           
	pstNetif = netif_eth_get_by_ipv6_prefix(ubaDestination, pubSource, pubNSAddr, blIsForSending, pubHopLimit);
	if (pstNetif)
		return pstNetif;
#endif

	//* 协议栈目前支持的网络接口类型为ppp及ethernet，ppp链路暂时不支持ipv6，ethernet路由选择由netif_get_eth_by_ipv6_prefix()函数完成（没有单独的路由表），所以这里只需直接返回NULL即可
	return pstNetif; 	
}

UCHAR *route_ipv6_get_source_ip(const UCHAR ubaDestination[16], UCHAR *pubSource)
{
	pubSource[0] = 0; 

#if SUPPORT_ETHERNET
	//*  同route_ipv6_get_netif()函数	                     
	PST_NETIF pstNetif = netif_eth_get_by_ipv6_prefix(ubaDestination, pubSource, NULL, FALSE, NULL);
	if (pstNetif)
		return pubSource;
#endif

	return NULL; 
}
#endif

#if NETTOOLS_TELNETSRV
const ST_ROUTE *route_get_next(const ST_ROUTE *pstNextRoute)
{    
    PST_ROUTE pstRoute = NULL; 

    os_thread_mutex_lock(l_hMtxRoute);
    {
        PST_ROUTE_NODE pstNextNode = l_pstRouteLink; 
        if (pstNextRoute)
        {
            while (pstNextNode)
            {
                if (pstNextRoute == &pstNextNode->stRoute)
                {
                    if (pstNextNode->pstNext)
                    {
                        pstRoute = &pstNextNode->pstNext->stRoute;
                        break;
                    }
                }                

                pstNextNode = pstNextNode->pstNext;
            }
        }
        else
        {
            if (pstNextNode)
                pstRoute = &pstNextNode->stRoute; 
        }
    }
    os_thread_mutex_unlock(l_hMtxRoute); 

    return pstRoute; 
}
#endif //* #if NETTOOLS_TELNETSRV

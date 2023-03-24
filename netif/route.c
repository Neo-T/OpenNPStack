/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
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

static ST_ROUTE_NODE l_staRouteNode[ROUTE_ITEM_NUM];
static PST_ROUTE_NODE l_pstFreeNode = NULL;
static PST_ROUTE_NODE l_pstRouteLink = NULL;
static HMUTEX l_hMtxRoute = INVALID_HMUTEX;

static ST_ROUTE_IPv6_NODE l_staRouteIpv6Node[ROUTE_IPv6_ITEM_NUM];
static PST_ROUTE_IPv6_NODE l_pstFreeNodeIpv6 = NULL;
static PST_ROUTE_IPv6_NODE l_pstRouteIpv6Link = NULL;
static HMUTEX l_hMtxRouteIpv6 = INVALID_HMUTEX;
BOOL route_table_init(EN_ONPSERR *penErr)
{
    //* 路由表清零
    memset(&l_staRouteNode[0], 0, sizeof(l_staRouteNode));

    //* 初始化
    INT i;
    for (i = 1; i < ROUTE_ITEM_NUM; i++)
        l_staRouteNode[i - 1].pstNext = &l_staRouteNode[i];
    l_pstFreeNode = &l_staRouteNode[0];

#if SUPPORT_IPV6
	for (i = 1; i < ROUTE_IPv6_ITEM_NUM; i++)
		l_staRouteIpv6Node[i - 1].pstNext = &l_staRouteIpv6Node[i];
	l_pstFreeNodeIpv6 = &l_staRouteIpv6Node[0];

	l_hMtxRouteIpv6 = os_thread_mutex_init();
	if (INVALID_HMUTEX == l_hMtxRouteIpv6)
	{
		if (penErr)
			*penErr = ERRMUTEXINITFAILED;
		return FALSE; 
	}
#endif
	
    l_hMtxRoute = os_thread_mutex_init();
    if (INVALID_HMUTEX != l_hMtxRoute)
        return TRUE;

#if SUPPORT_IPV6
	//* 失败了，归还刚才占用的资源
	os_thread_mutex_uninit(l_hMtxRouteIpv6); 
#endif

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

#if SUPPORT_IPV6
	l_pstRouteIpv6Link = NULL;
	l_pstFreeNodeIpv6 = NULL;

	if (INVALID_HMUTEX != l_hMtxRoute)
		os_thread_mutex_uninit(l_hMtxRouteIpv6);
#endif
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

PST_NETIF route_get_netif(UINT unDestination, BOOL blIsForSending, in_addr_t *punSrcIp, in_addr_t *punArpDstAddr)
{
    PST_ROUTE pstRoute = NULL/*PST_NETIF pstNetif = NULL*/;
    PST_ROUTE pstDefaultRoute = NULL/*PST_NETIF pstDefaultNetif = NULL*/;
    PST_NETIF pstNetif = NULL; 

#if SUPPORT_ETHERNET
    //* 先查找ethernet网卡链表（PPP链路不需要，因为这个只能按照既定规则发到拨号网络的对端），看看本地网段是否就可以满足要求，而不是需要查找路由表
    pstNetif = netif_get_eth_by_genmask(unDestination, punSrcIp, blIsForSending); 
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
    PST_NETIF pstNetif = netif_get_eth_by_genmask(unDestination, &unNetifIp, FALSE);
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
static PST_ROUTE_IPv6_NODE route_ipv6_get_free_node(void)
{
	PST_ROUTE_IPv6_NODE pstNode;
	os_thread_mutex_lock(l_hMtxRouteIpv6);
	{
		pstNode = l_pstFreeNodeIpv6;
		if (l_pstFreeNodeIpv6)
			l_pstFreeNodeIpv6 = l_pstFreeNodeIpv6->pstNext;
	}
	os_thread_mutex_unlock(l_hMtxRouteIpv6);

	memset(&pstNode->stRoute, 0, sizeof(pstNode->stRoute));
	return pstNode;
}

static void route_ipv6_put_free_node(PST_ROUTE_IPv6_NODE pstNode)
{
	os_thread_mutex_lock(l_hMtxRouteIpv6);
	{
		pstNode->pstNext = l_pstFreeNodeIpv6;
		l_pstFreeNodeIpv6 = pstNode;
	}
	os_thread_mutex_unlock(l_hMtxRouteIpv6);
}

BOOL route_ipv6_add(PST_NETIF pstNetif, UCHAR ubaDestination[16], UCHAR ubaGateway[16], UCHAR ubDestPrefixBitLen, EN_ONPSERR *penErr)
{
	PST_ROUTE_IPv6_NODE pstNode;

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	CHAR szIpv6[40]; 
#endif

	//* 先看看是否已经存在这个目标网段
	os_thread_mutex_lock(l_hMtxRouteIpv6);
	{
		PST_ROUTE_IPv6_NODE pstNextNode = l_pstRouteIpv6Link;
		while (pstNextNode)
		{
			if (!ipv6_addr_cmp(ubaDestination, pstNextNode->stRoute.ubaDestination, pstNextNode->stRoute.ubDestPrefixBitLen)) //* 目标网段相等，则只更新不增加新条目
			{
				memcpy(pstNextNode->stRoute.ubaSource, netif_get_source_ipv6_by_destination(pstNetif, ubaDestination), 16);
				memcpy(pstNextNode->stRoute.ubaGateway, ubaGateway, 16);
				pstNextNode->stRoute.ubDestPrefixBitLen = ubDestPrefixBitLen;
				pstNextNode->stRoute.pstNetif = pstNetif;
				pstNextNode->stRoute.pstNetif->bUsedCount = 0;
				pstNode = pstNextNode;
				os_thread_mutex_unlock(l_hMtxRouteIpv6);
				goto __lblEnd;
			}

			pstNextNode = pstNextNode->pstNext;
		}
	}
	os_thread_mutex_unlock(l_hMtxRouteIpv6);

	//* 执行到这里意味着没找到对应的Ipv6路由条目，需要新增一个
	pstNode = route_ipv6_get_free_node();
	if (NULL == pstNode)
	{
		if (penErr)
			*penErr = ERRNOROUTENODE;

		return FALSE;
	}

	//* 保存路由相关信息
	memcpy(pstNode->stRoute.ubaSource, netif_get_source_ipv6_by_destination(pstNetif, ubaDestination), 16);
	memcpy(pstNode->stRoute.ubaDestination, ubaDestination, 16);
	memcpy(pstNode->stRoute.ubaGateway, ubaGateway, 16);
	pstNode->stRoute.ubDestPrefixBitLen = ubDestPrefixBitLen;
	pstNode->stRoute.pstNetif = pstNetif;

	//* 加入链表
	os_thread_mutex_lock(l_hMtxRouteIpv6);
	{
		pstNode->pstNext = l_pstRouteIpv6Link;
		l_pstRouteIpv6Link = pstNode;
	}
	os_thread_mutex_unlock(l_hMtxRouteIpv6);
	pstNode->stRoute.pstNetif->bUsedCount = 0;

__lblEnd:
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1	
#if PRINTF_THREAD_MUTEX
	os_thread_mutex_lock(o_hMtxPrintf);
#endif
	printf("Add/Update network interface <%s> to IPv6 routing table\r\n[", pstNode->stRoute.pstNetif->szName);
	if (pstNode->stRoute.ubaDestination[0]) //* 缺省路由的地址为全零：::/0
	{
		printf("destination %s/%d", inet6_ntoa(pstNode->stRoute.ubaDestination, szIpv6), pstNode->stRoute.ubDestPrefixBitLen); 
	}
	else
		printf("destination default");	
	printf(", gateway %s]\r\n", inet6_ntoa(pstNode->stRoute.ubaGateway, szIpv6)); 
#if PRINTF_THREAD_MUTEX
	os_thread_mutex_unlock(o_hMtxPrintf);
#endif
#endif

	return TRUE;
}

void route_ipv6_del(UCHAR ubaDestination[16])
{
	PST_ROUTE_IPv6_NODE pstNode = NULL;

	//* 从路由表删除
	os_thread_mutex_lock(l_hMtxRouteIpv6);
	{
		PST_ROUTE_IPv6_NODE pstNextNode = l_pstRouteIpv6Link;
		PST_ROUTE_IPv6_NODE pstPrevNode = NULL;
		while (pstNextNode)
		{
			if (!ipv6_addr_cmp(ubaDestination, pstNextNode->stRoute.ubaDestination, pstNextNode->stRoute.ubDestPrefixBitLen))
			{
				if (pstPrevNode)
					pstPrevNode->pstNext = pstNextNode->pstNext;
				else
					l_pstRouteIpv6Link = pstNextNode->pstNext;
				pstNode = pstNextNode;
				break;
			}
			pstPrevNode = pstNextNode;
			pstNextNode = pstNextNode->pstNext;
		}
	}
	os_thread_mutex_unlock(l_hMtxRouteIpv6);

	if (pstNode)
		route_ipv6_put_free_node(pstNode);
}

void route_ipv6_del_ext(PST_NETIF pstNetif)
{
	PST_ROUTE_IPv6_NODE pstNode = NULL; 

	//* 从路由表删除
	os_thread_mutex_lock(l_hMtxRouteIpv6);
	{
		//* 等待网络接口使用计数清零，只有清零才可删除当前网络接口在路由表中的所有条目
		while (pstNetif->bUsedCount)
			os_sleep_ms(10);

		PST_ROUTE_IPv6_NODE pstNextNode = l_pstRouteIpv6Link;
		PST_ROUTE_IPv6_NODE pstPrevNode = NULL;
		while (pstNextNode)
		{
			if (pstNextNode->stRoute.pstNetif == pstNetif)
			{
				if (pstPrevNode)
					pstPrevNode->pstNext = pstNextNode->pstNext;
				else
					l_pstRouteIpv6Link = pstNextNode->pstNext;
				pstNode = pstNextNode->pstNext;

				//* 归还节点
				pstNextNode->pstNext = l_pstFreeNodeIpv6;
				l_pstFreeNodeIpv6 = pstNextNode;

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
	os_thread_mutex_unlock(l_hMtxRouteIpv6);
}

PST_NETIF route_ipv6_get_netif(UCHAR ubaDestination[16], BOOL blIsForSending, UCHAR *pubSource, UCHAR *pubNSAddr)
{
	PST_ROUTE_IPv6 pstRoute = NULL;
	PST_ROUTE_IPv6 pstDefaultRoute = NULL;
	PST_NETIF pstNetif = NULL;

#if SUPPORT_ETHERNET
	//* 先查找ethernet网卡链表（PPP链路不需要，因为这个只能按照既定规则发到拨号网络的对端），看看本地网段是否就可以满足要求，而不是需要查找路由表
	pstNetif = netif_get_eth_by_ipv6_prefix(ubaDestination, pubSource, blIsForSending);
	if (pstNetif)
		return pstNetif;
#endif

	//* 查找路由表
	os_thread_mutex_lock(l_hMtxRouteIpv6);
	{
		PST_ROUTE_IPv6_NODE pstNextNode = l_pstRouteIpv6Link;
		while (pstNextNode)
		{
			if (pstNextNode->stRoute.ubaDestination[0]) //* 缺省路由的地址为全零：::/0
			{
				if(!ipv6_addr_cmp(ubaDestination, pstNextNode->stRoute.ubaDestination, pstNextNode->stRoute.ubDestPrefixBitLen))
				{
					pstRoute = &pstNextNode->stRoute;
					break;
				}
			}
			else
				pstDefaultRoute = &pstNextNode->stRoute;

			pstNextNode = pstNextNode->pstNext;
		}

		if (!pstRoute)
			pstRoute = pstDefaultRoute; 

		//* 如果调用者调用该函数获取网络接口是用于发送，则需要对此进行计数，以确保使用期间该接口不会被删除
		if (blIsForSending && pstRoute)
			pstRoute->pstNetif->bUsedCount++;
	}
	os_thread_mutex_unlock(l_hMtxRouteIpv6);

	if (pstRoute)
	{
		if (pubSource)
			memcpy(pubSource, pstRoute->ubaSource, 16); 

		if (pubNSAddr)
			memcpy(pubNSAddr, pstRoute->ubaGateway, 16);

		pstNetif = pstRoute->pstNetif;
	}

	return pstNetif; 	
}

PST_NETIF route_ipv6_get_default(void)
{
	PST_ROUTE_IPv6 pstDefaultRoute = NULL;

	//* 查找路由表
	os_thread_mutex_lock(l_hMtxRouteIpv6);
	{
		PST_ROUTE_IPv6_NODE pstNextNode = l_pstRouteIpv6Link;
		while (pstNextNode)
		{
			if (!pstNextNode->stRoute.ubaDestination[0])
			{
				pstDefaultRoute = &pstNextNode->stRoute;
				break;
			}

			pstNextNode = pstNextNode->pstNext;
		}
	}
	os_thread_mutex_unlock(l_hMtxRouteIpv6);

	if (pstDefaultRoute)
		return pstDefaultRoute->pstNetif;
	else //* 缺省路由为空，则直接使用网络接口链表的首节点作为缺省路由
		return netif_get_first(FALSE);
}

UCHAR *route_ipv6_get_netif_ip(UCHAR ubaDestination[16], UCHAR ubaNetifIpv6[16])
{
	ubaNetifIpv6[0] = 0; 

#if SUPPORT_ETHERNET
	//* 查找本地ethernet网卡链表，先看看是否目标地址在同一个网段
	PST_NETIF pstNetif = netif_get_eth_by_ipv6_prefix(ubaDestination, ubaNetifIpv6, FALSE);
	if (pstNetif)
		return ubaNetifIpv6;
#endif

	//* 本地网段不匹配，只能走路由了
	os_thread_mutex_lock(l_hMtxRouteIpv6);
	{
		PST_ROUTE_IPv6_NODE pstNextNode = l_pstRouteIpv6Link;
		while (pstNextNode)
		{
			if (pstNextNode->stRoute.ubaDestination[0]) //* 缺省路由的地址为全零：::/0
			{
				if (!ipv6_addr_cmp(ubaDestination, pstNextNode->stRoute.ubaDestination, pstNextNode->stRoute.ubDestPrefixBitLen))
				{
					memcpy(ubaNetifIpv6, pstNextNode->stRoute.ubaSource, 16); 
					break;
				}
			}
			else
				memcpy(ubaNetifIpv6, pstNextNode->stRoute.ubaSource, 16); 			

			pstNextNode = pstNextNode->pstNext;
		}
	}
	os_thread_mutex_unlock(l_hMtxRouteIpv6);	

	return ubaNetifIpv6;
}
#endif

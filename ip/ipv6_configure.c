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
#include "mmu/buf_list.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "netif/netif.h"

#if SUPPORT_IPV6
#include "ip/ip.h"
#include "ip/icmpv6.h"
#define SYMBOL_GLOBALS
#include "ip/ipv6_configure.h"
#undef SYMBOL_GLOBALS

#if SUPPORT_ETHERNET

static ST_IPv6_DYNADDR l_staIpv6DynAddrs[IPV6_CFG_ADDR_NUM]; //* ipv6动态地址信息存储单元
static ST_IPv6_ROUTER l_staIpv6Routers[IPV6_ROUTER_NUM];     //* ipv6路由器信息存储单元
static CHAR l_bFreeIpv6DynAddrList = -1;  
static CHAR l_bFreeIpv6RouterList = -1; 

//* 动态地址及路由器相关存储单元链表初始化，其必须在进行实际的ipv6地址自动配置之前调用以准备好相关基础数据结构
void ipv6_cfg_init(void)
{
	CHAR i; 
	for (i = 0; i < IPV6_CFG_ADDR_NUM - 1; i++)	
		l_staIpv6DynAddrs[i].bNextAddr = i + 1; 
	l_staIpv6DynAddrs[i].bNextAddr = -1; 
	l_bFreeIpv6DynAddrList = 0; 

	for (i = 0; i < IPV6_ROUTER_NUM - 1; i++)
		l_staIpv6Routers[i].bNextRouter = i + 1;
	l_staIpv6Routers[i].bNextRouter = -1;
	l_bFreeIpv6RouterList = 0;
}

PST_IPv6_DYNADDR ipv6_dyn_addr_node_get(CHAR *pbNodeIdx, EN_ONPSERR *penErr) 
{
	PST_IPv6_DYNADDR pstFreeNode = (PST_IPv6_DYNADDR)array_linked_list_get(&l_bFreeIpv6DynAddrList, l_staIpv6DynAddrs, (UCHAR)sizeof(ST_IPv6_DYNADDR), offsetof(ST_IPv6_DYNADDR, bNextAddr), pbNodeIdx);
	if (!pstFreeNode)
	{
		if (penErr)
			*penErr = ERRNOIPv6DYNADDRNODE;
	}

	return pstFreeNode; 
}

void ipv6_dyn_addr_node_free(PST_IPv6_DYNADDR pstDynAddrNode)
{
	array_linked_list_put(pstDynAddrNode, &l_bFreeIpv6DynAddrList, l_staIpv6DynAddrs, (UCHAR)sizeof(ST_IPv6_DYNADDR), IPV6_CFG_ADDR_NUM, offsetof(ST_IPv6_DYNADDR, bNextAddr));
}

PST_IPv6_ROUTER ipv6_router_node_get(CHAR *pbNodeIdx, EN_ONPSERR *penErr)
{
	PST_IPv6_ROUTER pstFreeNode = (PST_IPv6_ROUTER)array_linked_list_get(&l_bFreeIpv6RouterList, l_staIpv6Routers, (UCHAR)sizeof(ST_IPv6_ROUTER), offsetof(ST_IPv6_ROUTER, bNextRouter), pbNodeIdx);
	if (!pstFreeNode)
	{
		if (penErr)
			*penErr = ERRNOIPv6ROUTERNODE;
	}

	return pstFreeNode;
}

void ipv6_router_node_free(PST_IPv6_ROUTER pstRouterNode)
{
	array_linked_list_put(pstRouterNode, &l_bFreeIpv6RouterList, l_staIpv6Routers, (UCHAR)sizeof(ST_IPv6_ROUTER), IPV6_ROUTER_NUM, offsetof(ST_IPv6_ROUTER, bNextRouter));
}

PST_IPv6_DYNADDR ipv6_dyn_addr_get(CHAR bDynAddr)
{
	if (bDynAddr >= 0 && bDynAddr < IPV6_CFG_ADDR_NUM)
		return &l_staIpv6DynAddrs[bDynAddr];
	else
		return NULL; 
}

PST_IPv6_ROUTER ipv6_router_get(CHAR bRouter)
{
	if (bRouter >= 0 && bRouter < IPV6_ROUTER_NUM)
		return &l_staIpv6Routers[bRouter];
	else
		return NULL;
}

const UCHAR *ipv6_router_get_addr(CHAR bRouter)
{
	if (bRouter >= 0 && bRouter < IPV6_ROUTER_NUM)
		return l_staIpv6Routers[bRouter].ubaAddr;
	else
		return NULL;
}

UCHAR ipv6_router_get_hop_limit(CHAR bRouter)
{
	if (bRouter >= 0 && bRouter < IPV6_ROUTER_NUM)
		return l_staIpv6Routers[bRouter].ubHopLimit; 
	else
		return 64;
}

CHAR ipv6_router_get_index(PST_IPv6_ROUTER pstRouter)
{
	return array_linked_list_get_index(pstRouter, l_staIpv6Routers, (UCHAR)sizeof(ST_IPv6_ROUTER), IPV6_ROUTER_NUM);
}

void netif_ipv6_dyn_addr_add(PST_NETIF pstNetif, PST_IPv6_DYNADDR pstDynAddr)
{	
	array_linked_list_put_tail(pstDynAddr, &pstNetif->stIPv6.bDynAddr, l_staIpv6DynAddrs, (UCHAR)sizeof(ST_IPv6_DYNADDR), IPV6_CFG_ADDR_NUM, offsetof(ST_IPv6_DYNADDR, bNextAddr));
}

void netif_ipv6_dyn_addr_del(PST_NETIF pstNetif, PST_IPv6_DYNADDR pstDynAddr)
{	
	array_linked_list_del(pstDynAddr, &pstNetif->stIPv6.bDynAddr, l_staIpv6DynAddrs, (UCHAR)sizeof(ST_IPv6_DYNADDR), offsetof(ST_IPv6_DYNADDR, bNextAddr)); 	
}

void netif_ipv6_router_add(PST_NETIF pstNetif, PST_IPv6_ROUTER pstRouter)
{
	array_linked_list_put_tail(pstRouter, &pstNetif->stIPv6.bRouter, l_staIpv6Routers, (UCHAR)sizeof(ST_IPv6_ROUTER), IPV6_ROUTER_NUM, offsetof(ST_IPv6_ROUTER, bNextRouter));
}

void netif_ipv6_router_del(PST_NETIF pstNetif, PST_IPv6_ROUTER pstRouter)
{
	array_linked_list_del(pstRouter, &pstNetif->stIPv6.bRouter, l_staIpv6Routers, (UCHAR)sizeof(ST_IPv6_ROUTER), offsetof(ST_IPv6_ROUTER, bNextRouter));	
}

/*
调用样例：
PST_IPv6_DYNADDR pstNextAddr;
CHAR bNextAddr = -1;
do {
	pstNextAddr = netif_ipv6_dyn_addr_next(pstNetif, &bNextAddr);
	if (pstNextAddr)
		printf("Ipv6 Dyna Addr: %p\r\n", pstNextAddr);
	else
	{
		printf("Ipv6 Dyna Addr is empty\r\n");
		break;
	}
} while (bNextAddr >= 0);
*/
PST_IPv6_DYNADDR netif_ipv6_dyn_addr_next(PST_NETIF pstNetif, CHAR *pbNextAddr)
{
	return (PST_IPv6_DYNADDR)array_linked_list_next(pbNextAddr, &pstNetif->stIPv6.bDynAddr, l_staIpv6DynAddrs, (UCHAR)sizeof(ST_IPv6_DYNADDR), offsetof(ST_IPv6_DYNADDR, bNextAddr)); 	
}


/*
与netif_ipv6_dyn_addr_next()函数不同，这是一个线程安全函数，其能够确保返回的地址节点在用户显式地调用netif_ipv6_dyn_addr_release()函数之前，
该节点占用的资源均不会被协议栈回收，即使生存时间到期，调用样例如下：
PST_IPv6_DYNADDR pstNextAddr = NULL;
do {
	pstNextAddr = netif_ipv6_dyn_addr_next_safe(pstNetif, pstNextAddr, TRUE); 
	if (pstNextAddr)
		printf("Ipv6 Dyna Addr: %p\r\n", pstNextAddr);
	else		
		printf("Ipv6 Dyna Addr is empty\r\n"); 		
} while (pstNextAddr);
*/
PST_IPv6_DYNADDR netif_ipv6_dyn_addr_next_safe(PST_NETIF pstNetif, PST_IPv6_DYNADDR pstPrevDynAddr, BOOL blIsRefCnt)
{
	PST_IPv6_DYNADDR pstNextDynAddr; 

	os_critical_init();

	os_enter_critical();
	{		
		pstNextDynAddr = (PST_IPv6_DYNADDR)array_linked_list_next_ext(pstPrevDynAddr, &pstNetif->stIPv6.bDynAddr, l_staIpv6DynAddrs, (UCHAR)sizeof(ST_IPv6_DYNADDR), offsetof(ST_IPv6_DYNADDR, bNextAddr));
		if (blIsRefCnt)
		{
			//* 当前取出的节点的引用计数加一
			if (pstNextDynAddr)
				pstNextDynAddr->i6a_ref_cnt++;

			//* 上一个节点的引用计数减一
			if (pstPrevDynAddr && pstPrevDynAddr->i6a_ref_cnt > 0)
				pstPrevDynAddr->i6a_ref_cnt--;
		}		
	}
	os_exit_critical();	

	return pstNextDynAddr; 
}

void netif_ipv6_dyn_addr_release(PST_IPv6_DYNADDR pstDynAddr)
{
	os_critical_init(); 

	os_enter_critical();
	{
		if (pstDynAddr->i6a_ref_cnt > 0)
			pstDynAddr->i6a_ref_cnt--;
	}
	os_exit_critical();
}

PST_IPv6_ROUTER netif_ipv6_router_next(PST_NETIF pstNetif, CHAR *pbNextRouter)
{
	return (PST_IPv6_ROUTER)array_linked_list_next(pbNextRouter, &pstNetif->stIPv6.bRouter, l_staIpv6Routers, (UCHAR)sizeof(ST_IPv6_ROUTER), offsetof(ST_IPv6_ROUTER, bNextRouter)); 
}

//* 其功能及存在的意义与netif_ipv6_dyn_addr_next()函数相同
PST_IPv6_ROUTER netif_ipv6_router_next_safe(PST_NETIF pstNetif, PST_IPv6_ROUTER pstPrevRouter, BOOL blIsRefCnt)
{
	PST_IPv6_ROUTER pstNextRouter;

	os_critical_init();

	os_enter_critical();
	{
		pstNextRouter = (PST_IPv6_ROUTER)array_linked_list_next_ext(pstPrevRouter, &pstNetif->stIPv6.bRouter, l_staIpv6Routers, (UCHAR)sizeof(ST_IPv6_ROUTER), offsetof(ST_IPv6_ROUTER, bNextRouter));
		if (blIsRefCnt)
		{
			if (pstNextRouter)
				pstNextRouter->i6r_ref_cnt++; 

			if (pstPrevRouter && pstPrevRouter->i6r_ref_cnt > 0)
				pstPrevRouter->i6r_ref_cnt--;
		}		
	}
	os_exit_critical();

	return pstNextRouter;
}

void netif_ipv6_router_release(PST_IPv6_ROUTER pstRouter)
{
	os_critical_init();

	os_enter_critical();
	{
		if (pstRouter->i6r_ref_cnt > 0)
			pstRouter->i6r_ref_cnt--; 
	}
	os_exit_critical();
}

//* 通过指定的ipv6地址查找已挂载的动态地址节点，如果参数blIsReleased为FALSE则这个函数的调用者在使用完这个地址节点后需要调用netif_ipv6_dyn_addr_release()函数进行主动释放
PST_IPv6_DYNADDR netif_ipv6_dyn_addr_get(PST_NETIF pstNetif, UCHAR ubaIpv6Addr[16], BOOL blIsReleased)
{
	PST_IPv6_DYNADDR pstNextDynAddr = NULL; 
	do {		
		pstNextDynAddr = netif_ipv6_dyn_addr_next_safe(pstNetif, pstNextDynAddr, TRUE);
		if (pstNextDynAddr)
		{
			if (!memcmp(pstNextDynAddr->ubaVal, ubaIpv6Addr, 16))
			{
				if (blIsReleased)
					netif_ipv6_dyn_addr_release(pstNextDynAddr);
				return pstNextDynAddr;
			}
		}
	} while (pstNextDynAddr);

	return NULL; 
}

//* 通过指定的ipv6地址查找已绑定到网卡上的路由器，注意这个函数的调用者在使用完这个路由器节点后需要调用netif_ipv6_router_release()函数释放掉
PST_IPv6_ROUTER netif_ipv6_router_get_by_addr(PST_NETIF pstNetif, UCHAR ubaRouterIpv6Addr[16])
{
	PST_IPv6_ROUTER pstNextRouter = NULL;
	do {
		//* 采用线程安全的函数读取路由器节点，直至调用netif_ipv6_router_release()函数之前，该节点占用的资源均不会被协议栈回收，即使生存时间到期
		pstNextRouter = netif_ipv6_router_next_safe(pstNetif, pstNextRouter, TRUE);
		if (pstNextRouter)
		{			
			if (!memcmp(pstNextRouter->ubaAddr, ubaRouterIpv6Addr, 16))
				return pstNextRouter;			
		}
	} while (pstNextRouter);

	return NULL; 
}

//* 检查网卡的所有地址节点，看看其是否已完成无状态地址自动配置
static BOOL netif_ipv6_dyn_addr_cfg_ended(PST_NETIF pstNetif)
{
	PST_IPv6_DYNADDR pstNextAddr = NULL; 
	CHAR bIsCfgEnded = TRUE; 

	do {
		//* 采用线程安全的函数读取地址节点，直至调用netif_ipv6_dyn_addr_release()函数之前，该节点占用的资源均不会被协议栈回收，即使生存时间到期
		pstNextAddr = netif_ipv6_dyn_addr_next_safe(pstNetif, pstNextAddr, TRUE);
		if (pstNextAddr && pstNextAddr->bitState == IPv6ADDR_TENTATIVE)
		{
			//* 处理完毕释放当前地址节点，其实就是引用计数减一
			netif_ipv6_dyn_addr_release(pstNextAddr);				

			bIsCfgEnded = FALSE; 
			break; 
		}
	} while (pstNextAddr); 

	return bIsCfgEnded; 
}

static BOOL netif_ipv6_dhcpv6_cfg_ended(PST_NETIF pstNetif)
{
	PST_IPv6_ROUTER pstNextRouter = NULL;
	CHAR bIsCfgEnded = TRUE;

	do {		
		pstNextRouter = netif_ipv6_router_next_safe(pstNetif, pstNextRouter, TRUE);
		if (pstNextRouter && pstNextRouter->bitDv6CfgState != Dv6CFG_END)
		{			
			netif_ipv6_router_release(pstNextRouter);

			bIsCfgEnded = FALSE;
			break;
		}
	} while (pstNextRouter);

	return bIsCfgEnded; 
}

static void netif_ipv6_dyn_addr_lifetime_decrement(PST_NETIF pstNetif)
{	
	PST_IPv6_DYNADDR pstNextAddr;
	CHAR bNext = -1;

	os_critical_init(); 

	//* 不需要保护，因为只有增加是在其它线程，但是增加操作是将地址增加到链表的尾部，完全不受影响，删除操作仅在本定时器，即使网卡DOWN操作，这里也会根据网卡状态清空地址列表后再结束使命
	do {
		pstNextAddr = netif_ipv6_dyn_addr_next(pstNetif, &bNext);
		if (pstNextAddr && pstNextAddr->bitState != IPv6ADDR_TENTATIVE)
		{
			os_enter_critical();
			{
				//* 既不是永生又不是寿命已到，此时需要对寿命进行递减
				if (pstNextAddr->unPreferredLifetime != 0xFFFFFFFF && pstNextAddr->unPreferredLifetime != 0)
				{
					pstNextAddr->unPreferredLifetime--;
					if (!pstNextAddr->unPreferredLifetime)
						pstNextAddr->bitState = IPv6ADDR_DEPRECATED; 
				}

				if (pstNextAddr->unValidLifetime != 0xFFFFFFFF && pstNextAddr->unValidLifetime != 0)
				{					
					pstNextAddr->unValidLifetime--;					
					if (pstNextAddr->unValidLifetime == IPv6ADDR_INVALID_TIME)
						pstNextAddr->bitState = IPv6ADDR_INVALID;
				}
			}			
			os_exit_critical();

			//* 先离开一次临界区，然后再进入后再判断是否需要删除，这样允许CPU被其它线程或中断抢占，特别是此时如果恰好有RA报文到达更新有效生存时间的话那就不用删除当前这个节点了
			os_enter_critical();
			{								
				if (!pstNextAddr->unValidLifetime && !pstNextAddr->i6a_ref_cnt) //* 地址已经无效了且未在使用，摘除并归还
				{					
					netif_ipv6_dyn_addr_del(pstNetif, pstNextAddr);
					ipv6_dyn_addr_node_free(pstNextAddr);
				}
			}
			os_exit_critical();
		}
		else
			break;
	} while (bNext >= 0);
}

static void netif_ipv6_router_lifetime_decrement(PST_NETIF pstNetif)
{
	PST_IPv6_ROUTER pstNextRouter;
	CHAR bNext = -1;

	os_critical_init();

	//* 同样不需要保护，参见netif_ipv6_dyn_addr_lifetime_decrement()函数注释
	do {
		pstNextRouter = netif_ipv6_router_next(pstNetif, &bNext);
		if (pstNextRouter && pstNextRouter->bitDv6CfgState == Dv6CFG_END) //* 只有配置完毕的才开始计时
		{
			os_enter_critical();
			{
				if (pstNextRouter->usLifetime != 0)
					pstNextRouter->usLifetime--; 
			}
			os_exit_critical(); 

			//*同样先离开再入临界区
			os_enter_critical(); 
			{
				if (!pstNextRouter->usLifetime && !pstNextRouter->i6r_ref_cnt) //* 路由器生存时间到期且未在使用，摘除并归还
				{
					netif_ipv6_router_del(pstNetif, pstNextRouter);
					ipv6_router_node_free(pstNextRouter); 
				}
			}
			os_exit_critical(); 
		}
		else
			break; 
	} while (bNext >= 0);
}

static BOOL netif_ipv6_addr_and_router_released(PST_NETIF pstNetif)
{
	PST_IPv6_DYNADDR pstNextAddr;
	PST_IPv6_ROUTER pstNextRouter;
	CHAR bNext = -1;
	CHAR bIsFreedOK = TRUE; 

	os_critical_init();

	//* 先归还地址节点
	do {
		pstNextAddr = netif_ipv6_dyn_addr_next(pstNetif, &bNext);
		if (pstNextAddr)
		{			
			os_enter_critical();
			{
				if (!pstNextAddr->i6a_ref_cnt)
				{
					netif_ipv6_dyn_addr_del(pstNetif, pstNextAddr);
					ipv6_dyn_addr_node_free(pstNextAddr);
				}
				else
					bIsFreedOK = FALSE; 
			}
			os_exit_critical();
		}
		else
			break;
	} while (bNext >= 0); 

	//* 归还路由器节点
	do {
		pstNextRouter = netif_ipv6_router_next(pstNetif, &bNext);
		if (pstNextRouter)
		{			
			os_enter_critical();
			{
				if (!pstNextRouter->i6r_ref_cnt)
				{
					netif_ipv6_router_del(pstNetif, pstNextRouter);
					ipv6_router_node_free(pstNextRouter);
				}
				else
					bIsFreedOK = FALSE; 
			}
			os_exit_critical();
		}
		else
			break;
	} while (bNext >= 0);

	return bIsFreedOK; 
}

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
//* 输出ipv6配置信息
static void print_ipv6_cfg_info(PST_NETIF pstNetif)
{
	//* 链路本地地址
	CHAR szIpv6[40];
#if PRINTF_THREAD_MUTEX
	os_thread_mutex_lock(o_hMtxPrintf);
#endif
	printf("\r\nConfiguration results of ipv6 for Ethernet adapter %s:\r\n", pstNetif->szName);
	printf("    Link-local address: %s\r\n", inet6_ntoa(pstNetif->nif_lla_ipv6, szIpv6));
#if PRINTF_THREAD_MUTEX
	os_thread_mutex_unlock(o_hMtxPrintf);
#endif

	PST_IPv6_DYNADDR pstNextAddr = NULL;
	do {
		//* 采用线程安全的函数读取地址节点，直至调用netif_ipv6_dyn_addr_release()函数之前，该节点占用的资源均不会被协议栈回收，即使生存时间到期
		pstNextAddr = netif_ipv6_dyn_addr_next_safe(pstNetif, pstNextAddr, TRUE);
		if (pstNextAddr && pstNextAddr->bitState != IPv6ADDR_TENTATIVE)
		{
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("    configured address: %s (%s, Valid lifetime %d)\r\n", inet6_ntoa(pstNextAddr->ubaVal, szIpv6), ipv6_addr_state(pstNextAddr->bitState), pstNextAddr->unValidLifetime);
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
		}
	} while (pstNextAddr);

	PST_IPv6_ROUTER pstNextRouter = NULL;
	do {
		pstNextRouter = netif_ipv6_router_next_safe(pstNetif, pstNextRouter, TRUE);
		if (pstNextRouter && pstNextRouter->bitDv6CfgState == Dv6CFG_END)
		{
			CHAR bRouter = ipv6_router_get_index(pstNextRouter); 
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("     default router<%d>: %s (%s, Lifetime %d)\r\n", bRouter, inet6_ntoa(pstNextRouter->ubaAddr, szIpv6), i6r_prf_desc(pstNextRouter->bitPrf), pstNextRouter->usLifetime);
			printf("            dns srv<%d>: %s\r\n", bRouter, inet6_ntoa(pstNextRouter->staDNSSrv[0].ubaAddr, szIpv6));			
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
			if (pstNextRouter->staDNSSrv[1].ubaAddr[0])
			{
			#if PRINTF_THREAD_MUTEX
				os_thread_mutex_lock(o_hMtxPrintf);
			#endif
				printf("                          %s\r\n", inet6_ntoa(pstNextRouter->staDNSSrv[1].ubaAddr, szIpv6));
			#if PRINTF_THREAD_MUTEX
				os_thread_mutex_unlock(o_hMtxPrintf);
			#endif
			}
		}
	} while (pstNextRouter);
}
#endif

static void ipv6_svv_timeout_handler(void *pvParam)
{
	PST_NETIF pstNetif = (PST_NETIF)pvParam; 

	//* 允许运行的状态
	if (pstNetif->stIPv6.bitSvvTimerState == IPv6SVVTMR_RUN)
	{
		//* 动态地址及缺省路由器生存计时器递减
		netif_ipv6_dyn_addr_lifetime_decrement(pstNetif); 
		netif_ipv6_router_lifetime_decrement(pstNetif); 
	}
	else //* 已被迁移到IPv6SVVTMR_STOP状态，此时将立即回收所有已占用的地址及路由器节点资源并归还生存定时器
	{
		//* 确定所有地址和路由器节点已被释放
		if (netif_ipv6_addr_and_router_released(pstNetif))
		{
			//* 所有节点已被释放，状态迁移到IPv6SVVTMR_RELEASED态，通知将状态迁移到IPv6SVVTMR_STOP态的“操作者”可以继续网卡的删除操作了，这里则直接结束当前计时器的使命
			pstNetif->stIPv6.bitSvvTimerState = IPv6SVVTMR_RELEASED; 
			return; 
		}
	}

	if (!one_shot_timer_new(ipv6_svv_timeout_handler, pstNetif, 1))
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("ipv6_svv_timeout_handler() failed, %s\r\n", onps_error(ERRNOIDLETIMER));
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
	}
}

//* ipv6地址自动配置状态机（one-shot定时器方式实现，每隔一秒检查一次状态并依据配置规则进行状态迁移），该定时器
//* 全权负责进行状态迁移操作，其它线程不会进行任何迁移操作
static void ipv6_cfg_timeout_handler(void *pvParam)
{	
	PST_NETIF pstNetif = (PST_NETIF)pvParam; 
	switch (pstNetif->stIPv6.bitCfgState)
	{
	case IPv6CFG_LNKADDR: 
		if (pstNetif->stIPv6.stLnkAddr.bitState != IPv6ADDR_TENTATIVE) //* 配置完成？
		{
			//* 迁移到路由器请求（RS）状态，发送路由器请求报文
			pstNetif->stIPv6.bitCfgState = IPv6CFG_RS; 
			icmpv6_send_rs(pstNetif, pstNetif->nif_lla_ipv6, NULL);
		}
		break; 

	case IPv6CFG_RS: 		
		if (pstNetif->stIPv6.bRouter < 0) //* 尚未收到任何路由器通告（RA）报文
		{
			pstNetif->stIPv6.bitOptCnt++;
			if (pstNetif->stIPv6.bitOptCnt < 3)
			{
				//* 继续发送路由器请求（RS）报文
				icmpv6_send_rs(pstNetif, pstNetif->nif_lla_ipv6, NULL);
			}
			else
			{
				pstNetif->stIPv6.bitCfgState = IPv6CFG_END; //* 至此配置结束，定时器的使命亦结束

		#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
				print_ipv6_cfg_info(pstNetif);
		#endif

				return;
			}
		}
		else
			pstNetif->stIPv6.bitCfgState = IPv6CFG_WAIT_DYNADDRCFG_END; //* 迁移到等待无状态地址配置结束，其实就是检索所有动态地址节点，所有节点地址状态非EN_IPv6ADDRSTATE::IPv6ADDR_TENTATIVE状态即可
		break; 

	case IPv6CFG_WAIT_DYNADDRCFG_END: 
		if (netif_ipv6_dyn_addr_cfg_ended(pstNetif))
			pstNetif->stIPv6.bitCfgState = IPv6CFG_WAIT_Dv6CFG_END; //* 迁移到等待所有路由器Dhcpv6配置结束的状态，同样检索所有缺省路由器节点，等待其配置状态为EN_DHCPv6CFGSTATE::Dv6CFG_END
		break; 

	case IPv6CFG_WAIT_Dv6CFG_END: 
		if (netif_ipv6_dhcpv6_cfg_ended(pstNetif))
		{
			pstNetif->stIPv6.bitCfgState = IPv6CFG_END; //* 至此配置结束，定时器的使命亦结束

		#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
			print_ipv6_cfg_info(pstNetif); 
		#endif

			return; 
		}
		break; 

	default:
		return; 
	}

	//* 启动生存时间定时器，放在这里的目的是必须确保这个定时器启动成功，否则占用的资源无法被回收，网卡删除操作相关的函数如ethernet_del()将被阻塞
	if (pstNetif->stIPv6.bitSvvTimerState == IPv6SVVTMR_INVALID && pstNetif->stIPv6.bRouter >= 0)
	{		
		if (one_shot_timer_new(ipv6_svv_timeout_handler, pstNetif, 1))
			pstNetif->stIPv6.bitSvvTimerState = IPv6SVVTMR_RUN; 
		else
		{		
	#if SUPPORT_PRINTF && DEBUG_LEVEL
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("Ipv6 survival timer failed to start, %s\r\n", onps_error(ERRNOIDLETIMER));
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
		}
	}	

	if (!one_shot_timer_new(ipv6_cfg_timeout_handler, pstNetif, 1)) 
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("ipv6_cfg_timeout_handler() failed, %s\r\n", onps_error(ERRNOIDLETIMER));
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif		
		return; 
	}
}

//* 地址冲突检测（DAD）计时函数
static void ipv6_cfg_dad_timeout_handler(void *pvParam)
{
	PST_NETIF pstNetif; 

	PST_IPv6_DYNADDR pstTentAddr = (PST_IPv6_DYNADDR)pvParam;
	UCHAR *pubAddr = (UCHAR *)pvParam; 
	if (IPv6LNKADDR_FLAG == pubAddr[15]) //* 地址的最后一个字节由特殊标志字段来区分地址类型：链路本地地址或动态地址
		pstNetif = (PST_NETIF)((UCHAR *)pubAddr - offsetof(ST_NETIF, stIPv6.stLnkAddr)); 	
	else
	{		
		PST_IPv6_ROUTER pstRouter = (PST_IPv6_ROUTER)ipv6_router_get((CHAR)pstTentAddr->bitRouter);
		if (pstRouter)		
			pstNetif = pstRouter->pstNetif;					
		else
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("ipv6_cfg_dad_timeout_handler() failed, router index out of range (0 - %d): %d\r\n", IPV6_ROUTER_NUM - 1, pstTentAddr->bitRouter);
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
			return; 
		}
	}

	//* 整套算法得以正常工作的基础是ST_IPv6_LNKADDR与ST_IPv6_DYNADDR的头部字段位长及存储顺序完全一致，覆盖结构体前17个字节
	switch (pstTentAddr->bitState)
	{
	case IPv6ADDR_TENTATIVE:
		pstTentAddr->bitOptCnt++;
		if (pstTentAddr->bitOptCnt < IPv6_DAD_TIMEOUT)
		{
			//* 存在冲突则重新生成地址继续试探
			if (pstTentAddr->bitConflict)
			{
				//* 重新生成尾部地址再次进行DAD
				UINT unNewTailAddr = rand_big();
				memcpy(pubAddr + 13, (UCHAR *)&unNewTailAddr, 2);
				pstTentAddr->bitOptCnt = 0;
				pstTentAddr->bitConflict = FALSE;
			}

			icmpv6_send_ns(pstNetif, NULL, pstTentAddr->ubaVal, NULL);    //* 继续发送试探报文，确保所有节点都能收到
			one_shot_timer_new(ipv6_cfg_dad_timeout_handler, pvParam, 1); //* 再次开启定时器继续“试探”
		}
		else
		{						
			//* 如果是动态地址，配置成功后还需要一些处理才能迁移地址状态
			if (IPv6LNKADDR_FLAG != pubAddr[15])
			{
				pstTentAddr->i6a_ref_cnt = 0; //* 引用计数清0，注意引用计数与bitOptCnt字段复用，此后其被用于显式地通知动态地址生存计时器当前地址是否正在被使用

				//* 这里临界保护的原因是读取这个值并进行状态迁移时存在携带前缀信息的RA报文被再次收到的情况，临界保护避免出现状态迁移冲突的问题
				os_critical_init();
				os_enter_critical();
				{
					if (pstTentAddr->unPreferredLifetime)
						pstTentAddr->bitState = IPv6ADDR_PREFERRED;  //* 地址状态迁移到“选用”状态
					else
						pstTentAddr->bitState = IPv6ADDR_DEPRECATED; //* 地址状态迁移到“弃用”状态
				}
				os_exit_critical();

				//netif_ipv6_dyn_addr_add(pstNetif, pstTentAddr);
			}
			else
				pstTentAddr->bitState = IPv6ADDR_PREFERRED;  //* 地址状态迁移到“选用”状态			
		}

		break;

	default:
		return;
	}
}

//* 开始ipv6地址自动配置
BOOL ipv6_cfg_start(PST_NETIF pstNetif, EN_ONPSERR *penErr)
{
	//* 初始ST_IPv6各关键字段状态
	pstNetif->stIPv6.bDynAddr = -1; 
	pstNetif->stIPv6.bRouter = -1; 
	pstNetif->stIPv6.bitCfgState = IPv6CFG_LNKADDR;
	pstNetif->stIPv6.bitSvvTimerState = IPv6SVVTMR_INVALID; 

#if 1
	//* 生成试探性的链路本地地址（Tentative Link Local Address）
	ipv6_lnk_addr(pstNetif, pstNetif->nif_lla_ipv6);
#else
	//* 测试ipv6链路本地地址冲突逻辑用，使用网络其它节点已经成功配置的地址来验证协议栈DAD逻辑是否正确，注意
	//* 测试的时候需要把IPv6LNKADDR_FLAG（ipv6_configure.h）值改成测试地址的最后一个字节值，比如在这里就是0xe3
	memcpy(pstNetif->nif_lla_ipv6, "\xfe\x80\x00\x00\x00\x00\x00\x00\xdd\x62\x1a\x01\xfa\xa0\xd0\xe3", 16);
#endif

	pstNetif->stIPv6.stLnkAddr.bitState = IPv6ADDR_TENTATIVE;

	//* 开启one-shot定时器用于地址自动配置，步长：1秒
	if (one_shot_timer_new(ipv6_cfg_timeout_handler, pstNetif, 1))			
		pstNetif->stIPv6.stLnkAddr.bitOptCnt = 0; 
	else
	{
		if (penErr)
			*penErr = ERRNOIDLETIMER;
		return FALSE; 
	}
	
	return ipv6_cfg_dad(pstNetif, &pstNetif->stIPv6.stLnkAddr, penErr);	
}

//* 开启重复地址检测
BOOL ipv6_cfg_dad(PST_NETIF pstNetif, void *pstTentAddr, EN_ONPSERR *penErr)
{
	//* 接下来要操作的字段PST_IPv6_LNKADDR与PST_IPv6_DYNADDR存储位置完全相同，所以这里直接使用其中一个作为参数pstTentAddr的确定的数据类型
	PST_IPv6_DYNADDR pstDynAddr = (PST_IPv6_DYNADDR)pstTentAddr;
	//pstDynAddr->bitState = IPv6ADDR_TENTATIVE;
	pstDynAddr->bitConflict = FALSE;
	pstDynAddr->bitOptCnt = 0;

	//* 开启DAD检测定时器，步长：1秒	
	if (!one_shot_timer_new(ipv6_cfg_dad_timeout_handler, pstTentAddr, 1))
	{
		if (penErr)
			*penErr = ERRNOIDLETIMER;
		return FALSE;
	}

	//* 发送邻居节点请求报文开启重复地址检测DAD（Duplicate Address Detect）以确定这个试探地址没有被其它节点使用
	if (icmpv6_send_ns(pstNetif, NULL, pstDynAddr->ubaVal, penErr) > 0)
		return TRUE;
	else
		return FALSE;
}
#endif
#endif

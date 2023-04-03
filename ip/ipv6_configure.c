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
static void ipv6_cfg_init(void)
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

PST_IPv6_DYNADDR ipv6_dyn_addr_node_get(EN_ONPSERR *penErr)
{
	PST_IPv6_DYNADDR pstFreeNode = (PST_IPv6_DYNADDR)array_linked_list_get(&l_bFreeIpv6DynAddrList, l_staIpv6DynAddrs, (UCHAR)sizeof(ST_IPv6_DYNADDR), offsetof(ST_IPv6_DYNADDR, bNextAddr)); 
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

PST_IPv6_ROUTER ipv6_router_node_get(EN_ONPSERR *penErr)
{
	PST_IPv6_ROUTER pstFreeNode = (PST_IPv6_ROUTER)array_linked_list_get(&l_bFreeIpv6RouterList, l_staIpv6Routers, (UCHAR)sizeof(ST_IPv6_ROUTER), offsetof(ST_IPv6_ROUTER, bNextRouter));
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
	os_enter_critical();
	{
		if (pstRouter->i6r_ref_cnt > 0)
			pstRouter->i6r_ref_cnt--; 
	}
	os_exit_critical();
}

//* 通过指定的ipv6地址查找已绑定到网卡上的路由器
PST_IPv6_ROUTER netif_ipv6_router_get_by_addr(PST_NETIF pstNetif, UCHAR ubaRouterIpv6Addr)
{
	PST_IPv6_ROUTER pstNextRouter = NULL;
	do {
		//* 采用线程安全的函数读取地址节点，直至调用netif_ipv6_dyn_addr_release()函数之前，该节点占用的资源均不会被协议栈回收，即使生存时间到期
		pstNextRouter = netif_ipv6_router_next_safe(pstNetif, pstNextRouter, TRUE);
		if (pstNextRouter)
		{			
			if (!memcmp(pstNextRouter->ubaAddr, ubaRouterIpv6Addr, 16))
				return pstNextRouter; 
		}
	} while (pstNextRouter);
}

//* ipv6地址自动配置状态机（one-shot定时器方式实现，每隔一秒检查一次状态并依据配置规则进行状态迁移），该定时器
//* 全权负责进行状态迁移操作，其它线程不会进行任何迁移操作
static void ipv6_cfg_timeout_handler(void *pvParam)
{	
	PST_NETIF pstNetif = (PST_NETIF)pvParam; 
	switch (pstNetif->stIPv6.bitCfgState)
	{
	case IPv6CFG_LNKADDR: 
		if (pstNetif->stIPv6.stLnkAddr.bitState == IPv6ADDR_PREFERRED) //* 配置完成？
		{
			//* 迁移到路由器请求（RS）状态，发送路由器请求报文
			pstNetif->stIPv6.bitCfgState = IPv6CFG_RS; 
			icmpv6_send_rs(pstNetif, pstNetif->stIPv6.stLnkAddr.ubaVal, NULL); 
		}
		break; 

	case IPv6CFG_RS: 
		break; 

	default:
		return; 
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
			printf("ipv6_cfg_dad_timeout_handler() failed, router index out of range (0 - %d): %d\r\n", IPV6_ROUTER_NUM, pstTentAddr->bitRouter);
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
				memcpy(pubAddr + 13, (UCHAR *)&unNewTailAddr, 3);
				pstTentAddr->bitOptCnt = 0;
				pstTentAddr->bitConflict = FALSE;
			}

			icmpv6_send_ns(pstNetif, NULL, pstTentAddr->ubaVal, NULL);    //* 继续发送试探报文，确保所有节点都能收到
			one_shot_timer_new(ipv6_cfg_dad_timeout_handler, pvParam, 1); //* 再次开启定时器
		}
		else
		{						
			//* 如果是动态地址配置成功后还需要一些操作才能将地址状态迁移到“选用”
			if (IPv6LNKADDR_FLAG != pubAddr[15])
			{
				pstTentAddr->i6a_ref_cnt = 0; //* 引用计数清0，注意引用计数与bitOptCnt字段复用，此后其被用于显式地通知动态地址生存计时器当前地址是否正在被使用
				//netif_ipv6_dyn_addr_add(pstNetif, pstTentAddr);
			}

			pstTentAddr->bitState = IPv6ADDR_PREFERRED;
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

	//* 生成试探性的链路本地地址（Tentative Link Local Address）
	//icmpv6_lnk_addr_get(pstNetif, pstNetif->stIPv6.stLnkAddr.ubaAddr); 

	//* 测试ipv6链路本地地址冲突逻辑用，使用网络其它节点已经成功配置的地址来验证协议栈DAD逻辑是否正确
	memcpy(pstNetif->stIPv6.stLnkAddr.ubaVal, "\xfe\x80\x00\x00\x00\x00\x00\x00\xdd\x62\x1a\x01\xfa\xa0\xd0\xe3", 16); 	

	//* 开启one-shot定时器用于地址自动配置，步长：1秒
	if (one_shot_timer_new(ipv6_cfg_timeout_handler, pstNetif, 1))			
		pstNetif->stIPv6.stLnkAddr.bitOptCnt = 0; 
	else
	{
		if (penErr)
			*penErr = ERRNOIDLETIMER;
		return FALSE; 
	}

	//* 显式地通知后续的处理函数这是链路本地地址不是根据路由器发布的前缀生成或dhcpv6服务器分配的动态地址
	pstNetif->stIPv6.stLnkAddr.ubaVal[15] = IPv6LNKADDR_FLAG;	
	return ipv6_cfg_dad(pstNetif, &pstNetif->stIPv6.stLnkAddr, penErr);	
}

//* 开启重复地址检测
BOOL ipv6_cfg_dad(PST_NETIF pstNetif, void *pstTentAddr, EN_ONPSERR *penErr)
{
	//* 接下来要操作的字段PST_IPv6_LNKADDR与PST_IPv6_DYNADDR存储位置完全相同，所以这里直接使用其中一个作为参数pstTentAddr的确定的数据类型
	PST_IPv6_DYNADDR pstDynAddr = (PST_IPv6_DYNADDR)pstTentAddr;
	pstDynAddr->bitState = IPv6ADDR_TENTATIVE;
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

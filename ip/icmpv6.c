/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
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
#define SYMBOL_GLOBALS
#include "ip/icmpv6.h"
#undef SYMBOL_GLOBALS

#define MULTICAST_ADDR_NUMM 5 //* 协议栈支持的组播地址数量
static const UCHAR l_ubaNetifNodesMcAddr[16] = { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };			 //* FF01::1，接口本地范围内所有节点组播地址
static const UCHAR l_ubaAllNodesMcAddr[16] = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };			 //* FF02::1，链路本地范围内所有节点组播地址
static const UCHAR l_ubaSolNodeMcAddrPrefix[IPv6MCA_SOLPREFIXBITLEN / 8] = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF }; //* FF02::1:FF00:0/104 链路本地范围内的邻居节点请求组播地址前缀
static const UCHAR l_ubaAllRoutersMcAddr[16] = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };			 //* FF02::2，链路本地范围内所有路由器组播地址
static const UCHAR l_ubaAllMLDv2RoutersMcAddr[16] = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16 };		 //* FF02::16，所有支持MLDv2路由器的组播地址
static const UCHAR *l_pubaMcAddr[MULTICAST_ADDR_NUMM] = { l_ubaNetifNodesMcAddr, l_ubaAllNodesMcAddr, l_ubaSolNodeMcAddrPrefix, l_ubaAllRoutersMcAddr, l_ubaAllMLDv2RoutersMcAddr }; //* 存储顺序必须与EN_MCADDR_TYPE的定义顺序一致


static const UCHAR l_ubaLinkLocalAddrPrefix[IPv6LLA_PREFIXBITLEN / 8] = { 0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; //* FE80::/64，链路本地地址前缀，其组成形式为：FE80::/64 + EUI-64地址


#if SUPPORT_ETHERNET
 //* ipv6到以太网Mac地址映射表，该表仅缓存最近通讯的主机映射，缓存数量由sys_config.h中IPV6TOMAC_ENTRY_NUM宏指定
static STCB_ETHIPv6MAC l_stcbaEthIpv6Mac[ETHERNET_NUM];

//* icmpv6支持的无状态(stateless)地址配置定时器溢出函数
static void ipv6_addr_cfg_timeout_handler(void *pvParam)
{
	PST_NETIF pstNetif = (PST_NETIF)pvParam; 
	switch ((EN_IPv6CFGSTATE)pstNetif->stIPv6.bitState)
	{
	case IPv6CFG_LATENT:
		pstNetif->stIPv6.bitTimingCnt++; 
		if (pstNetif->stIPv6.bitTimingCnt < 4)
		{
			//* 存在冲突则重新生成地址继续试探
			if (pstNetif->stIPv6.bitIsLAConflict)
			{
				UINT unNewTailAddr = rand_big();
				memcpy(&pstNetif->stIPv6.ubaLnkAddr[13], (UCHAR *)&unNewTailAddr, 3);
				pstNetif->stIPv6.bitTimingCnt = 0;
				pstNetif->stIPv6.bitIsLAConflict = FALSE; 

				CHAR szIpv6[40]; 
				os_thread_mutex_lock(o_hMtxPrintf);
				printf("Conflict: %s\r\n", inet6_ntoa(pstNetif->stIPv6.ubaLnkAddr, szIpv6)); 
				os_thread_mutex_unlock(o_hMtxPrintf);
			}

			//* 继续发送试探报文，确保所有节点都能收到
			icmpv6_send_ns(pstNetif, NULL, pstNetif->stIPv6.ubaLnkAddr, NULL); 

			one_shot_timer_new(ipv6_addr_cfg_timeout_handler, pstNetif, 1); 
		}
		else
		{
			os_thread_mutex_lock(o_hMtxPrintf);
			printf("Not conflict!\r\n");
			os_thread_mutex_unlock(o_hMtxPrintf);
		}
		break; 

	default:
		break;
	}
}

//* 等待邻居节点回馈的NA（Neighbor Advertisement）应答报文定时器溢出函数，其实就是实现ipv4的arp定时器相同的功能，通过ipv6地址得到mac地址
static void icmpv6_na_wait_timeout_handler(void *pvParam)
{

}

void ipv6_mac_mapping_tbl_init(void)
{
	memset(l_stcbaEthIpv6Mac, 0, sizeof(l_stcbaEthIpv6Mac));

	//* 组成等待队列资源链
	INT k;
	for (k = 0; k < ETHERNET_NUM; k++)
	{
		INT i;
		for (i = 0; i < TCPSRV_BACKLOG_NUM_MAX - 1; i++)
			l_stcbaEthIpv6Mac[k].staSListWaitQueue[i].pstNext = &l_stcbaEthIpv6Mac[k].staSListWaitQueue[i + 1];
		l_stcbaEthIpv6Mac[k].staSListWaitQueue[i].pstNext = NULL;
		l_stcbaEthIpv6Mac[k].pstSListWaitQueueFreed = &l_stcbaEthIpv6Mac[k].staSListWaitQueue[0];
		l_stcbaEthIpv6Mac[k].pstSListWaitQueue = NULL;
	}
}

//* 申请一个映射表控制块给上层应用
PSTCB_ETHIPv6MAC ipv6_mac_ctl_block_new(void)
{
	PSTCB_ETHIPv6MAC pstcbIpv6Mac = NULL;
	os_critical_init();

	//* 申请一个新的控制块    
	os_enter_critical();
	{
		INT i;
		for (i = 0; i < ETHERNET_NUM; i++)
		{
			if (!l_stcbaEthIpv6Mac[i].bIsUsed)
			{
				l_stcbaEthIpv6Mac[i].bIsUsed = TRUE;
				pstcbIpv6Mac = &l_stcbaEthIpv6Mac[i];
				pstcbIpv6Mac->bLastReadEntryIdx = 0;
				pstcbIpv6Mac->bEntriesNum = 0; 
				break;
			}
		}
	}
	os_exit_critical();

	return pstcbIpv6Mac;
}

//* 释放映射表控制块
void ipv6_mac_ctl_block_free(PSTCB_ETHIPv6MAC pstcbIpv6Mac)
{
	memset(pstcbIpv6Mac, 0, sizeof(STCB_ETHIPv6MAC));
	pstcbIpv6Mac->bIsUsed = FALSE; 
}

void ipv6_mac_add_entry(PST_NETIF pstNetif, UCHAR ubaIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], BOOL blIsOverride)
{	
	PSTCB_ETHIPv6MAC pstcbIpv6Mac = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbIpv6Mac; 

	os_critical_init();

	//* 先查找该条目是否已经存在，如果存在则直接更新，否则直接添加或替换最老条目
	INT i;
	for (i = 0; i < pstcbIpv6Mac->bEntriesNum/*IPV6TOMAC_ENTRY_NUM*/; i++) 
	{			
		if (!ipv6_addr_cmp(ubaIpv6, pstcbIpv6Mac->staEntry[i].ubaIpv6, 128)) //* 匹配
		{
			if (blIsOverride)
			{
				//* 更新mac地址
				os_enter_critical();
				{
					memcpy(pstcbIpv6Mac->staEntry[i].ubaMac, ubaMacAddr, ETH_MAC_ADDR_LEN);
					pstcbIpv6Mac->staEntry[i].unUpdateTime = os_get_system_secs();
				}
				os_exit_critical();
			}			
			return;
		}
	}

	//* 到这里意味着尚未缓存该地址条目，需要增加一条或者覆盖最老的一个条目
	if (i < IPV6TOMAC_ENTRY_NUM)
		pstcbIpv6Mac->bEntriesNum++;
	else
	{
		INT nFirstEntry = 0;
		for (i = 1; i < IPV6TOMAC_ENTRY_NUM; i++)
		{
			if (pstcbIpv6Mac->staEntry[nFirstEntry].unUpdateTime > pstcbIpv6Mac->staEntry[i].unUpdateTime)
				nFirstEntry = i;
		}

		i = nFirstEntry;
	}
		

	//* 更新mac地址
	os_enter_critical();
	{
		memcpy(pstcbIpv6Mac->staEntry[i].ubaMac, ubaMacAddr, ETH_MAC_ADDR_LEN);
		pstcbIpv6Mac->staEntry[i].unUpdateTime = os_get_system_secs();
		memcpy(pstcbIpv6Mac->staEntry[i].ubaIpv6, ubaIpv6, 16); 
	}
	os_exit_critical();

__lblSend:
	//* 逐个取出待发送报文
	os_enter_critical();
	{
		//* 看看有没有待发送的报文
		PST_SLINKEDLIST_NODE pstNextNode = (PST_SLINKEDLIST_NODE)pstcbIpv6Mac->pstSListWaitQueue;
		PST_SLINKEDLIST_NODE pstPrevNode = NULL; 
		while (pstNextNode)
		{
			PSTCB_ETHIPv6MAC_WAIT pstcbIpv6MacWait = (PSTCB_ETHIPv6MAC_WAIT)pstNextNode->uniData.ptr;			
			if (!ipv6_addr_cmp(ubaIpv6, pstcbIpv6MacWait->ubaIpv6, 128)) //* 匹配，可以发送这个报文了
			{
				//* 通知定时器这个节点正处于发送状态，不要释放占用的内存
				pstcbIpv6MacWait->ubSndStatus = 1; 

				//* 首先摘除这个节点并归还给空闲资源队列，以便第一时间释放临界区
				if (pstPrevNode)
					pstPrevNode->pstNext = pstNextNode->pstNext;
				else
					pstcbIpv6Mac->pstSListWaitQueue = pstNextNode->pstNext;
				sllist_put_node(&pstcbIpv6Mac->pstSListWaitQueueFreed, pstcbIpv6MacWait->pstNode);
				pstcbIpv6MacWait->pstNode = NULL; //* 清空，显式地告诉定时器已经发送了（如果定时器溢出函数此时已经执行的话）
				os_exit_critical();

				UCHAR *pubIpPacket = ((UCHAR *)pstcbIpv6MacWait) + sizeof(STCB_ETHIPv6MAC_WAIT); 

				//* 申请一个buf list节点并将ip报文挂载到list上
				EN_ONPSERR enErr;
				SHORT sBufListHead = -1;
				SHORT sIpPacketNode = buf_list_get_ext(pubIpPacket, (UINT)pstcbIpv6MacWait->usIpPacketLen, &enErr); 
				if (sIpPacketNode < 0)
				{
			#if SUPPORT_PRINTF && DEBUG_LEVEL
				#if PRINTF_THREAD_MUTEX
					os_thread_mutex_lock(o_hMtxPrintf);
				#endif
					printf("ipv6_mac_add_entry() failed, %s\r\n", onps_error(enErr));
				#if PRINTF_THREAD_MUTEX
					os_thread_mutex_unlock(o_hMtxPrintf);
				#endif
			#endif                    
				}
				buf_list_put_head(&sBufListHead, sIpPacketNode);

				//* 完成实际地发送
				if (pstNetif->pfunSend(pstNetif, IPV4, sBufListHead, ubaMacAddr, &enErr) < 0)
				{
			#if SUPPORT_PRINTF && DEBUG_LEVEL
				#if PRINTF_THREAD_MUTEX
					os_thread_mutex_lock(o_hMtxPrintf);
				#endif
					printf("ipv6_mac_add_entry() failed, %s\r\n", onps_error(enErr));
				#if PRINTF_THREAD_MUTEX
					os_thread_mutex_unlock(o_hMtxPrintf);
				#endif
			#endif
				}
				pstcbIpv6MacWait->ubSndStatus = 2;	//* 完成发送，通知定时器可以释放占用地内存了
				buf_list_free(sIpPacketNode);		//* 释放buf list节点

				goto __lblSend;
			}

			//* 下一个节点
			pstPrevNode = pstNextNode;
			pstNextNode = pstNextNode->pstNext;
		}
	}
	os_exit_critical();
}

void ipv6_mac_add_entry_ext(PSTCB_ETHIPv6MAC pstcbIpv6Mac, UCHAR ubaIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], BOOL blIsOverride)
{
	os_critical_init();

	//* 先查找该条目是否已经存在，如果存在则直接更新，否则直接添加或替换最老条目
	INT i;
	for (i = 0; i < pstcbIpv6Mac->bEntriesNum; i++)
	{		
		if (!ipv6_addr_cmp(ubaIpv6, pstcbIpv6Mac->staEntry[i].ubaIpv6, 128)) //* 匹配
		{
			if (blIsOverride)
			{
				//* 更新mac地址
				os_enter_critical();
				{
					memcpy(pstcbIpv6Mac->staEntry[i].ubaMac, ubaMacAddr, ETH_MAC_ADDR_LEN);
					pstcbIpv6Mac->staEntry[i].unUpdateTime = os_get_system_secs();
				}
				os_exit_critical();
			}			

			return;
		}		
	}

	//* 到这里意味着尚未缓存该地址条目，需要增加一条或者覆盖最老的一个条目
	if (i < IPV6TOMAC_ENTRY_NUM)
		pstcbIpv6Mac->bEntriesNum++;
	else
	{
		INT nFirstEntry = 0;
		for (i = 1; i < IPV6TOMAC_ENTRY_NUM; i++)
		{
			if (pstcbIpv6Mac->staEntry[nFirstEntry].unUpdateTime > pstcbIpv6Mac->staEntry[i].unUpdateTime)
				nFirstEntry = i;
		}

		i = nFirstEntry;
	}

	//* 更新mac地址
	os_enter_critical();
	{
		memcpy(pstcbIpv6Mac->staEntry[i].ubaMac, ubaMacAddr, ETH_MAC_ADDR_LEN);
		pstcbIpv6Mac->staEntry[i].unUpdateTime = os_get_system_secs();
		memcpy(pstcbIpv6Mac->staEntry[i].ubaIpv6, ubaIpv6, 16);
	}
	os_exit_critical();
}

static INT ipv6_to_mac(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN])
{
	PSTCB_ETHIPv6MAC pstcbIpv6Mac = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbIpv6Mac;

	//* 如果目标Ipv6地址为组播地址则依据ipv6标准生成组播mac地址。组播地址的判断依据是Ipv6地址以FF开头（P83 3.5）：
	//* |---8位---|-4位-|-4位-|--------------112位--------------|
	//* |1111 1111|标记 |范围 |              组ID               |
	//* 标记：0RPT，T位为0表示这个是由IANA分配的固定且众所周知的组播地址，反之则是临时地址；P位表示其是否基于一个单播地址前缀（参见RFC3306）；R位表示其是否含有内嵌的交汇点地址（参见RFC3956）；
	//* 范围：指定组播数据需要被发往哪个Ipv6网络范围，路由器以此来判定组播报文能否发送出去，相关值定义如下：
	//*       0，保留；1，接口本地范围；2，链路本地范围；3，保留；4，管理本地范围；5，站点本地范围；8，组织本地范围；E，全球范围；F，保留；
	//* 组ID：标识组播组，其值在地址范围内是唯一的。同样其也存在一些被永久分配的组ID：
	//*       FF01::1，接口本地范围内的所有节点组播地址；
	//*       FF02::1，链路本地范围内的所有节点组播地址；
	//*       FF01::2，接口本地范围内的所有路由组播地址；
	//*       FF02::2，链路本地范围内的所有路由组播地址；
	//*       FF05::2，站点本地范围内的所有路由组播地址；
	//*       最新IANA分配的永久组播地址详见：https://www.iana.org/assignments/ipv6-multicast-addresses
	//* 
	//* ipv6组播地址到mac组播地址的转换规则如下（P85 3.5.2）：
	//*                       96位     104位    112位    120位  127位
	//*                        :        :        :        :      :
	//* ipv6组播地址 0xFF.....:|||||||| |||||||| |||||||| ||||||||
	//*      mac地址 0x33-0x33-||||||||-||||||||-||||||||-||||||||

	//* 如果ip地址为广播地址则填充目标mac地址也为广播地址
	if (ubaDstIpv6[0] == 0xFF)
	{
		ubaMacAddr[0] = IPv6MCTOMACADDR_PREFIX;
		ubaMacAddr[1] = IPv6MCTOMACADDR_PREFIX;
		memcpy(&ubaMacAddr[2], &ubaDstIpv6[12], 4);
		return 0;
	}

	os_critical_init();

	//* 是否命中最近刚读取过的条目
	os_enter_critical();
	if (!ipv6_addr_cmp(ubaDstIpv6, pstcbIpv6Mac->staEntry[pstcbIpv6Mac->bLastReadEntryIdx].ubaIpv6, 128))
	{
		memcpy(ubaMacAddr, pstcbIpv6Mac->staEntry[pstcbIpv6Mac->bLastReadEntryIdx].ubaMac, ETH_MAC_ADDR_LEN);
		pstcbIpv6Mac->staEntry[pstcbIpv6Mac->bLastReadEntryIdx].unUpdateTime = os_get_system_secs();
		os_exit_critical();
		return 0;
	}
	os_exit_critical();

	//* 未命中，则查找整个缓存表
	INT i;
	for (i = 0; i < (INT)pstcbIpv6Mac->bEntriesNum; i++)
	{
		os_enter_critical();
		if (!ipv6_addr_cmp(ubaDstIpv6, pstcbIpv6Mac->staEntry[i].ubaIpv6, 128))
		{
			memcpy(ubaMacAddr, pstcbIpv6Mac->staEntry[i].ubaMac, ETH_MAC_ADDR_LEN);
			pstcbIpv6Mac->staEntry[i].unUpdateTime = os_get_system_secs();
			pstcbIpv6Mac->bLastReadEntryIdx = i;
			os_exit_critical();
			return 0;
		}
		os_exit_critical();
	}

	return 1; 
}

static PSTCB_ETHIPv6MAC_WAIT ipv6_mac_wait_packet_put(PST_NETIF pstNetif, UCHAR ubaDstIpv6[16], SHORT sBufListHead, BOOL *pblNetifFreedEn, EN_ONPSERR *penErr)
{
	PSTCB_ETHIPv6MAC pstcbIpv6Mac = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbIpv6Mac; 
	UINT unIpPacketLen = buf_list_get_len(sBufListHead); //* 获取报文长度
	PSTCB_ETHIPv6MAC_WAIT pstcbIpv6MacWait = (PSTCB_ETHIPv6MAC_WAIT)buddy_alloc(sizeof(STCB_ETHIPv6MAC_WAIT) + unIpPacketLen, penErr); //* 申请一块缓冲区用来缓存当前尚无法发送的报文并记录当前报文的相关控制信息
	if (pstcbIpv6MacWait)
	{
		UCHAR *pubIpPacket = ((UCHAR *)pstcbIpv6MacWait) + sizeof(STCB_ETHIPv6MAC_WAIT); 

		//* 保存报文到刚申请的内存中，然后开启一个1秒定时器等待arp查询结果并在得到正确mac地址后发送ip报文
		buf_list_merge_packet(sBufListHead, pubIpPacket); 

		//* 计数器清零，并传递当前选择的netif
		pstcbIpv6MacWait->pstNetif = pstNetif;
		memcpy(pstcbIpv6MacWait->ubaIpv6, ubaDstIpv6, 16);		
		pstcbIpv6MacWait->usIpPacketLen = (USHORT)unIpPacketLen;
		pstcbIpv6MacWait->ubCount = 0;
		pstcbIpv6MacWait->ubSndStatus = 0;
		pstcbIpv6MacWait->pstNode = NULL; 

		//* 启动一个1秒定时器，等待查询完毕
		pstcbIpv6MacWait->pstTimer = one_shot_timer_new(icmpv6_na_wait_timeout_handler, pstcbIpv6MacWait, 1);
		if (pstcbIpv6MacWait->pstTimer)
		{
			*pblNetifFreedEn = FALSE; 

			os_critical_init();
			os_enter_critical();
			{
				PST_SLINKEDLIST_NODE pstNode = sllist_get_node(&pstcbIpv6Mac->pstSListWaitQueueFreed);
				if (pstNode)
				{
					pstNode->uniData.ptr = pstcbIpv6MacWait;
					pstcbIpv6MacWait->pstNode = pstNode;
					sllist_put_tail_node(&pstcbIpv6Mac->pstSListWaitQueue, pstNode);
				}
			}
			os_exit_critical();
		}
		else
		{
			//* 定时器未启动，这里就要释放刚才申请的内存
			buddy_free(pstcbIpv6MacWait);

			if (penErr)
				*penErr = ERRNOIDLETIMER;
			pstcbIpv6MacWait = NULL;
		}
	}

	return pstcbIpv6MacWait; 
}

INT ipv6_mac_get(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], EN_ONPSERR *penErr)
{
	if (ipv6_to_mac(pstNetif, ubaSrcIpv6, ubaDstIpv6, ubaMacAddr))
	{
		//* 不存在，则只能发送一条邻居节点地址请求报文问问谁拥有这个ipv6地址了	
		if (icmpv6_send_ns(pstNetif, ubaSrcIpv6, ubaDstIpv6, penErr) < 0)
			return -1;
		return 1;
	}
	else
		return 0; 	
}

INT ipv6_mac_get_ext(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], SHORT sBufListHead, BOOL *pblNetifFreedEn, EN_ONPSERR *penErr)
{
	if (ipv6_to_mac(pstNetif, ubaSrcIpv6, ubaDstIpv6, ubaMacAddr))
	{
		//* 先将这条报文放入待发送链表		
		PSTCB_ETHIPv6MAC_WAIT pstcbIpv6MacWait = ipv6_mac_wait_packet_put(pstNetif, ubaDstIpv6, sBufListHead, pblNetifFreedEn, penErr);
		if (!pstcbIpv6MacWait)
			return -1;

		//* 发送一条邻居节点地址请求报文问问谁拥有这个ipv6地址
		if (icmpv6_send_ns(pstNetif, ubaSrcIpv6, ubaDstIpv6, penErr) < 0)
		{
			one_shot_timer_free(pstcbIpv6MacWait->pstTimer);
			return -1;
		}

		return 1;
	}
	else
		return 0;
}

const UCHAR *icmpv6_lnk_addr_get(PST_NETIF pstNetif, UCHAR ubaLnkAddr[16])
{
	PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;

	//* 生成链路本地地址，首先复制地址前缀
	memcpy(ubaLnkAddr, l_ubaLinkLocalAddrPrefix, sizeof(l_ubaLinkLocalAddrPrefix));

	//* 复制以太网mac地址前24位
	UCHAR ubUField = pstExtra->ubaMacAddr[0];
	memcpy(&ubaLnkAddr[sizeof(l_ubaLinkLocalAddrPrefix)], pstExtra->ubaMacAddr, 3);
	//* 按照惯例在这里将mac地址U/L位取反以提供最大程度的可压缩性（mac地址U位为1时取反为0则可提高ipv6地址连续全0字段的概率，因为
	//* 协议栈运行环境基本都是自行指定mac地址，此时U位应为1以显式地告知这是本地mac地址）
	ubUField = (ubUField & 0xFD) | (~ubUField & 0x02);
	ubaLnkAddr[sizeof(l_ubaLinkLocalAddrPrefix)] = ubUField; 
	ubaLnkAddr[sizeof(l_ubaLinkLocalAddrPrefix) + 3] = 0xFF; 
	ubaLnkAddr[sizeof(l_ubaLinkLocalAddrPrefix) + 4] = 0xFE; 
	memcpy(&ubaLnkAddr[sizeof(l_ubaLinkLocalAddrPrefix) + 5], &pstExtra->ubaMacAddr[3], 3); 

	return ubaLnkAddr; 
}

BOOL icmpv6_start_config(PST_NETIF pstNetif, EN_ONPSERR *penErr)
{
	//* 冲突标志清零
	pstNetif->stIPv6.bitIsLAConflict = pstNetif->stIPv6.bitIsUAConflict = pstNetif->stIPv6.bitIsTAConflict = 0; 

	//* 生成试探性的链路本地地址（Tentative Link Local Address）
	//icmpv6_lnk_addr_get(pstNetif, pstNetif->stIPv6.ubaLnkAddr); 
	memcpy(pstNetif->stIPv6.ubaLnkAddr, "\xfe\x80\x00\x00\x00\x00\x00\x00\xdd\x62\x1a\x01\xfa\xa0\xd0\xe3", 16); //* 测试ipv6链路本地地址冲突逻辑用，使用网络其它节点已经成功配置的地址来验证协议栈DAD逻辑是否正确
	pstNetif->stIPv6.bitLAPrefixBitLen = IPv6LLA_PREFIXBITLEN; 

	//* 开启one-shot定时器，步长：1秒	
	if (one_shot_timer_new(ipv6_addr_cfg_timeout_handler, pstNetif, 1))
	{
		pstNetif->stIPv6.bitState = IPv6CFG_LATENT;
		pstNetif->stIPv6.bitTimingCnt = 0; 		
	}
	else
	{
		if (penErr)
			*penErr = ERRNOIDLETIMER;
		return FALSE; 
	}

	//* 开始重复地址检测DAD（Duplicate Address Detect）以确定这个试探地址没有被其它节点使用
	if (icmpv6_send_ns(pstNetif, NULL, pstNetif->stIPv6.ubaLnkAddr, penErr) > 0)
		return TRUE;
	else
		return FALSE; 
}
#endif

const UCHAR *ipv6_mc_addr(EN_IPv6MCADDR_TYPE enType)
{
	if (0 <= (INT)enType < MULTICAST_ADDR_NUMM)
		return l_pubaMcAddr[enType];
	else
		return NULL;
}

UCHAR *ipv6_sol_mc_addr(UCHAR ubaUniIpv6[16], UCHAR ubaSolMcAddr[16])
{
	//* 生成节点请求组播地址
	UCHAR ubPrefixBytes = IPv6MCA_SOLPREFIXBITLEN / 8;
	memcpy(ubaSolMcAddr, ipv6_mc_addr(IPv6MCA_SOLNODE), ubPrefixBytes);
	memcpy(&ubaSolMcAddr[ubPrefixBytes], &ubaUniIpv6[ubPrefixBytes], 3);

	return ubaSolMcAddr;
}

static INT icmpv6_send(PST_NETIF pstNetif, UCHAR ubType, UCHAR ubCode, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], SHORT sBufListHead, UINT unFlowLabel, UCHAR ubHopLimit, EN_ONPSERR *penErr)
{
	ST_IPv6_PSEUDOHDR stPseudoHdr;	
	ST_ICMPv6_HDR stHdr;
	UINT unIcmpv6DataLen = buf_list_get_len(sBufListHead); 

	//* 申请一个buf list节点	
	SHORT sIcmpv6HdrNode = buf_list_get_ext(&stHdr, (UINT)sizeof(ST_ICMPv6_HDR), penErr);
	if (sIcmpv6HdrNode < 0)
		return -1;
	buf_list_put_head(&sBufListHead, sIcmpv6HdrNode);

	//* 申请ip伪报头并将其挂载到链表头部以便计算icmpv6校验和
	SHORT sPseudoHdrNode;
	sPseudoHdrNode = buf_list_get_ext((UCHAR *)&stPseudoHdr, (UINT)sizeof(ST_IPv6_PSEUDOHDR), penErr);
	if (sPseudoHdrNode < 0)
	{
		buf_list_free(sIcmpv6HdrNode);
		return -1;
	}
	buf_list_put_head(&sBufListHead, sPseudoHdrNode);

	//* 封装icmpv6 ns报文
	//* ================================================================================
	stHdr.ubType = ubType; 
	stHdr.ubCode = ubCode;
	stHdr.usChecksum = 0;	
	//* ================================================================================

	//* 封装ip伪报头用于icmpv6校验和计算
	//* ================================================================================
	if (ubaSrcIpv6)
		memcpy(stPseudoHdr.ubaSrcIpv6, ubaSrcIpv6, 16);
	else
		memset(stPseudoHdr.ubaSrcIpv6, 0, sizeof(stPseudoHdr.ubaSrcIpv6)); 

	switch(ubType)
	{
	case ICMPv6_NS: //* Neighbor Solicitation，邻居请求，根据目的地址生成组播地址：FF02::1:FF00:0/104 + 目的地址的后24位		
		memcpy(stPseudoHdr.ubaDstIpv6, l_ubaSolNodeMcAddrPrefix, sizeof(l_ubaSolNodeMcAddrPrefix));
		memcpy(&stPseudoHdr.ubaDstIpv6[sizeof(l_ubaSolNodeMcAddrPrefix)], &ubaDstIpv6[sizeof(l_ubaSolNodeMcAddrPrefix)], 3);
		break; 

	default: 
		memcpy(stPseudoHdr.ubaDstIpv6, ubaDstIpv6, 16); 
		break; 
	}	

	stPseudoHdr.unIpv6PayloadLen = htonl(sizeof(ST_ICMPv6_HDR) + unIcmpv6DataLen);
	stPseudoHdr.ubaMustBeZero[0] = stPseudoHdr.ubaMustBeZero[1] = stPseudoHdr.ubaMustBeZero[2] = 0;
	stPseudoHdr.ubProto = IPPROTO_ICMPv6;
	//* ================================================================================

	//* 计算校验和并释放伪报头
	stHdr.usChecksum = tcpip_checksum_ext(sBufListHead); 
	buf_list_free_head(&sBufListHead, sPseudoHdrNode); 

	//* 发送，并释放占用的buf list节点
	INT nRtnVal = ipv6_send(pstNetif, NULL, stPseudoHdr.ubaSrcIpv6, stPseudoHdr.ubaDstIpv6, IPPROTO_ICMPv6, sBufListHead, unFlowLabel, ubHopLimit, penErr);
	buf_list_free(sIcmpv6HdrNode); 

	CHAR szIpv6[40]; 
	os_thread_mutex_lock(o_hMtxPrintf);
	printf("ns_src_addr: %s\r\n", inet6_ntoa(stPseudoHdr.ubaSrcIpv6, szIpv6));
	printf("ns_dst_addr: %s\r\n", inet6_ntoa(stPseudoHdr.ubaDstIpv6, szIpv6));
	os_thread_mutex_unlock(o_hMtxPrintf);

	return nRtnVal; 
}

INT icmpv6_send_ns(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], EN_ONPSERR *penErr)
{	
	PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;

	UCHAR ubaNSolicitation[sizeof(ST_ICMPv6_NS_HDR) + sizeof(ST_ICMPv6_OPT_LLA)]; 	
	PST_ICMPv6_NS_HDR pstNeiSolHdr = (PST_ICMPv6_NS_HDR)ubaNSolicitation; 
	PST_ICMPv6_OPT_LLA pstOptLnkAddr = (PST_ICMPv6_OPT_LLA)&ubaNSolicitation[sizeof(ST_ICMPv6_NS_HDR)]; 

	//* 申请一个buf list节点
	SHORT sBufListHead = -1;
	SHORT sIcmpv6NeiSolNode = buf_list_get_ext(ubaNSolicitation, ubaSrcIpv6 ? (UINT)sizeof(ubaNSolicitation) : (UINT)sizeof(ST_ICMPv6_NS_HDR), penErr);
	if (sIcmpv6NeiSolNode < 0)
		return -1;
	buf_list_put_head(&sBufListHead, sIcmpv6NeiSolNode);

	//* 封装icmpv6 ns报文
	//* ================================================================================	
	pstNeiSolHdr->unReserved = 0; 
	memcpy(pstNeiSolHdr->ubaTargetAddr, ubaDstIpv6, 16); 
	if (ubaSrcIpv6)
	{
		pstOptLnkAddr->ubType = ICMPV6OPT_SLLA;
		pstOptLnkAddr->ubLen = 1; //* 单位：8字节
		memcpy(pstOptLnkAddr->ubaAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN);
	}	
	//* ================================================================================

	//* 完成实际的发送并释放占用的buf list节点
	INT nRtnVal = icmpv6_send(pstNetif, ICMPv6_NS, 0, ubaSrcIpv6, ubaDstIpv6, sBufListHead, 0, 255, penErr);
	buf_list_free(sIcmpv6NeiSolNode); 

	return nRtnVal; 
}

//* 收到的NA（Neighbor Advertisement，邻居通告）报文处理函数
void icmpv6_na_handler(PST_NETIF pstNetif, UCHAR *pubIcmpv6)
{
	PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
	PST_ICMPv6_NA_HDR pstNeiAdvHdr = (PST_ICMPv6_NA_HDR)(pubIcmpv6 + sizeof(ST_ICMPv6_HDR));
	PST_ICMPv6_OPT_LLA pstOptLnkAddr = (PST_ICMPv6_OPT_LLA)(pubIcmpv6 + sizeof(ST_ICMPv6_HDR) + sizeof(ST_ICMPv6_NA_HDR)); 
	pstNeiAdvHdr->icmpv6_na_flag = htonl(pstNeiAdvHdr->icmpv6_na_flag);

	//* 首先看看Solicited标志是否置位，是，意味着这是NS报文的应答报文
	if (pstNeiAdvHdr->icmpv6_na_flag_s)
	{
		//* Override标志置位？，更新ipv6 to mac缓存映射表，注意只有主动请求的响应报文才会操作缓存映射表，这样可避免频繁的通告导致缓存表无法保持相对稳定的问题
		if (pstNeiAdvHdr->icmpv6_na_flag_o)
			ipv6_mac_add_entry_ext(pstExtra->pstcbIpv6Mac, pstNeiAdvHdr->ubaTargetAddr, pstOptLnkAddr->ubaAddr, TRUE);
		else
			ipv6_mac_add_entry_ext(pstExtra->pstcbIpv6Mac, pstNeiAdvHdr->ubaTargetAddr, pstOptLnkAddr->ubaAddr, FALSE); //* 只增加或覆盖老的条目，已有条目不更新

		//* 收到请求通告，icmpv6_na_wait_timeout_handler()溢出函数不必执行溢出操作了
	}
	else
	{
		//* 这是一个邻居节点通告，且是面向链路上所有节点（目标地址为FF02::1），所以只要是仍在地址自动配置阶段就需要判断是否存在地址冲突，因为
		//* 存在某个节点响应很慢的情形，导致DAD检测结束才收到冲突报文，虽然概率很低，但为了可靠必须处理这种情况
		if (pstNetif->stIPv6.bitState < IPv6CFG_END)
		{
			if (pstNetif->stIPv6.ubaTmpAddr[0] && !memcmp(pstNeiAdvHdr->ubaTargetAddr, pstNetif->stIPv6.ubaTmpAddr, 16) && !pstNetif->stIPv6.bitIsTAConflict)
				pstNetif->stIPv6.bitIsTAConflict = TRUE;
			else if (pstNetif->stIPv6.ubaUniAddr[0] && !memcmp(pstNeiAdvHdr->ubaTargetAddr, pstNetif->stIPv6.ubaUniAddr, 16) && !pstNetif->stIPv6.bitIsUAConflict)
				pstNetif->stIPv6.bitIsUAConflict = TRUE;
			else if (!memcmp(pstNeiAdvHdr->ubaTargetAddr, pstNetif->stIPv6.ubaLnkAddr, 16) && !pstNetif->stIPv6.bitIsLAConflict)
				pstNetif->stIPv6.bitIsLAConflict = TRUE;
			else; //* 其它节点地址冲突导致的全节点通告，不用理会
		}		
	}
}

void icmpv6_recv(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR *pubPacket, INT nPacketLen, UCHAR *pubIcmpv6)
{
	ST_IPv6_PSEUDOHDR stPseudoHdr; //* 用于校验和计算
	PST_IPv6_HDR pstIpv6Hdr = (PST_IPv6_HDR)pubPacket;
	PST_ICMPv6_HDR pstIcmpv6Hdr = (PST_ICMPv6_HDR)pubIcmpv6; 
	EN_ONPSERR enErr; 
	USHORT usIpv6PayloadLen = htons(pstIpv6Hdr->usPayloadLen);

	//* 校验收到的整个报文确定其完整、可靠
	//* =================================================================================
	//* 申请一个buf list节点将icmpv6报文挂载到链表上
	SHORT sBufListHead = -1; 
	SHORT sIpv6PayloadNode = buf_list_get_ext(pubIcmpv6, (UINT)usIpv6PayloadLen, &enErr);
	if (sIpv6PayloadNode < 0)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL > 0		
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("icmpv6_recv() failed, %s\r\n", onps_error(enErr)); 
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
		return; 
	}
	buf_list_put_head(&sBufListHead, sIpv6PayloadNode);

	//* 申请一个buf list节点将ipv6伪报头挂载到链表头部
	SHORT sPseudoHdrNode;
	sPseudoHdrNode = buf_list_get_ext((UCHAR *)&stPseudoHdr, (UINT)sizeof(ST_IPv6_PSEUDOHDR), &enErr);
	if (sPseudoHdrNode < 0)
	{	
		buf_list_free(sIpv6PayloadNode);

#if SUPPORT_PRINTF && DEBUG_LEVEL > 0		
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("icmpv6_recv() failed, %s\r\n", onps_error(enErr));		
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
		return;
	}
	buf_list_put_head(&sBufListHead, sPseudoHdrNode);

	//* 封装ipv6伪报头
	memcpy(stPseudoHdr.ubaSrcIpv6, pstIpv6Hdr->ubaSrcIpv6, 16); 
	memcpy(stPseudoHdr.ubaDstIpv6, pstIpv6Hdr->ubaDstIpv6, 16); 
	stPseudoHdr.unIpv6PayloadLen = htonl((UINT)usIpv6PayloadLen);
	stPseudoHdr.ubaMustBeZero[0] = stPseudoHdr.ubaMustBeZero[1] = stPseudoHdr.ubaMustBeZero[2] = 0;
	stPseudoHdr.ubProto = IPPROTO_ICMPv6; 		

	//* 计算校验和
	USHORT usPktChecksum = pstIcmpv6Hdr->usChecksum;
	pstIcmpv6Hdr->usChecksum = 0; 
	USHORT usChecksum = tcpip_checksum_ext(sBufListHead);
	buf_list_free_head(&sBufListHead, sPseudoHdrNode); 
	buf_list_free(sIpv6PayloadNode); 

	//* 未通过数据校验，则直接丢弃当前报文
	if (usPktChecksum != usChecksum)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL > 3
		pstIcmpHdr->usChecksum = usPktChecksum;
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("checksum error (%04X, %04X), the icmpv6 packet will be dropped\r\n", usChecksum, usPktChecksum);
		printf_hex(pubPacket, nPacketLen, 48);
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
		return;
	}
	//* =================================================================================

	switch ((EN_ICMPv6TYPE)pstIcmpv6Hdr->ubType)
	{
	case ICMPv6_NS: 
		printf("++++++++recv NS packet\r\n"); 
		break; 

	case ICMPv6_NA:
		icmpv6_na_handler(pstNetif, pubIcmpv6);
		break; 

	default: 
		printf("++++++++recv %d packet\r\n", pstIcmpv6Hdr->ubType);
		break; 
	}
}
#endif

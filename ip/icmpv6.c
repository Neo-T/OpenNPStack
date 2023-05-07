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
#define SYMBOL_GLOBALS
#include "ip/icmpv6.h"
#undef SYMBOL_GLOBALS
#include "ip/ipv6_configure.h"

#if SUPPORT_ETHERNET
#include "ethernet/dhcpv6.h"
#endif
//* 详细的组播地址汇总及最新更新详见：https://www.iana.org/assignments/ipv6-multicast-addresses/ipv6-multicast-addresses.xhtml#node-local
#define MULTICAST_ADDR_NUMM 5 //* 协议栈支持的组播地址数量
static const UCHAR l_ubaNetifNodesMcAddr[16] = { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };			 //* FF01::1，接口本地范围内所有节点组播地址
static const UCHAR l_ubaAllNodesMcAddr[16] = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };			 //* FF02::1，链路本地范围内所有节点组播地址
static const UCHAR l_ubaSolNodeMcAddrPrefix[IPv6MCA_SOLPREFIXBITLEN / 8] = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF }; //* FF02::1:FF00:0/104 链路本地范围内的邻居节点请求组播地址前缀
static const UCHAR l_ubaAllRoutersMcAddr[16] = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };			 //* FF02::2，链路本地范围内所有路由器组播地址
static const UCHAR l_ubaAllMLDv2RoutersMcAddr[16] = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16 };		 //* FF02::16，所有支持MLDv2路由器的组播地址
static const UCHAR *l_pubaMcAddr[MULTICAST_ADDR_NUMM] = { l_ubaNetifNodesMcAddr, l_ubaAllNodesMcAddr, l_ubaSolNodeMcAddrPrefix, l_ubaAllRoutersMcAddr, l_ubaAllMLDv2RoutersMcAddr }; //* 存储顺序必须与EN_MCADDR_TYPE的定义顺序一致

//* 关于链路本地地址及其它ipv6地址分类详见：https://www.rfc-editor.org/rfc/rfc3513 链路本地地址详见section-2.5.6
static const UCHAR l_ubaLinkLocalAddrPrefix[8] = { 0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; //* FE80::/64，链路本地地址前缀，其组成形式为：FE80::/64 + EUI-64地址


#if SUPPORT_ETHERNET
//* ipv6到以太网Mac地址映射表，该表仅缓存最近通讯的主机映射，缓存数量由sys_config.h中IPV6TOMAC_ENTRY_NUM宏指定
static STCB_ETHIPv6MAC l_stcbaEthIpv6Mac[ETHERNET_NUM];

//* 等待邻居节点回馈的NA（Neighbor Advertisement）应答报文定时器溢出函数，其实就是实现ipv4的arp定时器相同的功能，通过ipv6地址得到mac地址
static void icmpv6_na_wait_timeout_handler(void *pvParam)
{
	PSTCB_ETHIPv6MAC_WAIT pstcbIpv6MacWait = (PSTCB_ETHIPv6MAC_WAIT)pvParam;
	PST_NETIF pstNetif = pstcbIpv6MacWait->pstNetif;
	EN_ONPSERR enErr;
	UCHAR *pubIpPacket;
	PST_IPv6_HDR pstIpv6Hdr;
	UCHAR ubaDstMac[ETH_MAC_ADDR_LEN];
	INT nRtnVal;

	os_critical_init();

	//* 尚未发送
	if (!pstcbIpv6MacWait->ubSndStatus)
	{
		//* 查询计数，如果超出限值，则不再查询直接丢弃该报文
		pstcbIpv6MacWait->ubCount++;
		if (pstcbIpv6MacWait->ubCount > 5)
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("The neighbor solicitation times out and the packet will be dropped\r\n"); 
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
			goto __lblEnd;
		}
	}
	else //* 已经处于发送中或发送完成状态，直接跳到函数尾部
		goto __lblEnd; 

	//* 此时已经过去了1秒，看看此刻是否已经得到目标ethernet网卡的mac地址
	pubIpPacket = ((UCHAR *)pstcbIpv6MacWait) + sizeof(STCB_ETHIPv6MAC_WAIT);
	pstIpv6Hdr = (PST_IPv6_HDR)pubIpPacket;	         
	nRtnVal = ipv6_mac_get(pstNetif, pstIpv6Hdr->ubaSrcIpv6, pstcbIpv6MacWait->ubaIpv6, ubaDstMac, &enErr);
	if (!nRtnVal) //* 存在该条目，则直接调用ethernet接口注册的发送函数即可
	{
		os_enter_critical();
		{
			//* 尚未发送，则首先摘除这个节点
			if (pstcbIpv6MacWait->pstNode)
			{
				PSTCB_ETHIPv6MAC pstcbIpv6Mac = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbIpv6Mac;
				sllist_del_node(&pstcbIpv6Mac->pstSListWaitQueue, pstcbIpv6MacWait->pstNode);       //* 从队列中删除
				sllist_put_node(&pstcbIpv6Mac->pstSListWaitQueueFreed, pstcbIpv6MacWait->pstNode);  //* 放入空闲资源队列
				pstcbIpv6MacWait->pstNode = NULL; //* 清空，显式地告知后续地处理代码这个节点已经被释放
			}
			else //* 已经发送，则没必要重复发送
			{
				os_exit_critical();
				goto __lblEnd;
			}
		}
		os_exit_critical();

		//* 申请一个buf list节点并将ipv6报文挂载到list上
		SHORT sBufListHead = -1;
		SHORT sIpPacketNode = buf_list_get_ext(pubIpPacket, (UINT)pstcbIpv6MacWait->usIpPacketLen, &enErr);
		if (sIpPacketNode < 0)
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("icmpv6_na_wait_timeout_handler() failed, %s\r\n", onps_error(enErr));
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
			goto __lblEnd;
		}
		buf_list_put_head(&sBufListHead, sIpPacketNode);

		//* 完成实际地发送   
		nRtnVal = pstNetif->pfunSend(pstNetif, IPV6, sBufListHead, ubaDstMac, &enErr);
		if (nRtnVal < 0)
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("icmpv6_na_wait_timeout_handler() failed, %s\r\n", onps_error(enErr));
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
		}
		buf_list_free(sIpPacketNode); //* 释放buf list节点
	}
	else
	{
		//* 说明还是没有得到mac地址，需要再次开启一个定时器等1秒后再发送一次试试
		if (nRtnVal > 0 && one_shot_timer_new(icmpv6_na_wait_timeout_handler, pstcbIpv6MacWait, 1))
			return;
		else
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("icmpv6_na_wait_timeout_handler() failed, %s\r\n", nRtnVal < 0 ? onps_error(enErr) : onps_error(ERRNOIDLETIMER));
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
		}
	}


__lblEnd:	
	//* 必须在判断发送状态值之前摘除节点，否则会出现icmpv6的接收线程在收到mac解析地址后重新发送数据过程中网卡及内存被释放的情形
	os_enter_critical();
	{
		//* 超时或者出错了，不再继续请求了，直接摘除这个节点，至此ipv6_mac_add_entry()函数不再会取到这个节点，接下来释放网卡及内存的操作才会安全
		if (pstcbIpv6MacWait->pstNode)
		{
			PSTCB_ETHIPv6MAC pstcbIpv6Mac = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbIpv6Mac; 			
			sllist_del_node(&pstcbIpv6Mac->pstSListWaitQueue, pstcbIpv6MacWait->pstNode);       //* 从队列中删除
			sllist_put_node(&pstcbIpv6Mac->pstSListWaitQueueFreed, pstcbIpv6MacWait->pstNode);  //* 放入空闲资源队列
			pstcbIpv6MacWait->pstNode = NULL; 
		}
	}
	os_exit_critical();

	//* 经过了上面的再次摘除操作，在这里再次判断发送状态，如果不处于发送状态则直接释放内存，否则再次开启定时器以待发送完成后释放占用的内存
	if (1 == pstcbIpv6MacWait->ubSndStatus)
		one_shot_timer_new(icmpv6_na_wait_timeout_handler, pstcbIpv6MacWait, 1);
	else
	{
		netif_freed(pstNetif); //* 不再占用网卡		
		buddy_free(pvParam);   //* 归还报文占用的内存
	}
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
				if (pstNetif->pfunSend(pstNetif, IPV6, sBufListHead, ubaMacAddr, &enErr) < 0)
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
	//* ipv6组播地址到mac组播地址的转换规则如下（P85 3.5.2），其实就是把组播地址的后32为作为mac地址后32位，如下：
	//*                       96位     104位    112位    120位  127位
	//*                        :        :        :        :      :
	//* ipv6组播地址 0xFF.....:|||||||| |||||||| |||||||| ||||||||
	//*      mac地址 0x33-0x33-||||||||-||||||||-||||||||-||||||||

	//* 如果ip地址为广播地址则按照上述规则填充目标mac地址为广播地址
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

//* 关于链路本地地址前缀的说明：https://www.rfc-editor.org/rfc/rfc3513#section-2.5.6
//* 生成链路本地地址：FE80::/64 + EUI-64地址
const UCHAR *ipv6_lnk_addr(PST_NETIF pstNetif, UCHAR ubaLnkAddr[16])
{
	PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;

	//* 生成链路本地地址，首先复制地址前缀
	memcpy(ubaLnkAddr, l_ubaLinkLocalAddrPrefix, 8);

	//* 复制以太网mac地址前24位
	UCHAR ubUField = pstExtra->ubaMacAddr[0];
	memcpy(&ubaLnkAddr[8], pstExtra->ubaMacAddr, 3);

	//* 按照惯例在这里将mac地址U/L位取反以提供最大程度的可压缩性（mac地址U位为1时取反为0则可提高ipv6地址连续全0字段的概率，因为
	//* 协议栈运行环境基本都是自行指定mac地址，此时U位应为1以显式地告知这是本地mac地址）
	ubUField = (ubUField & 0xFD) | (~ubUField & 0x02);
	ubaLnkAddr[8] = ubUField; 
	ubaLnkAddr[11] = 0xFF; 
	ubaLnkAddr[12] = 0xFE; 
	memcpy(&ubaLnkAddr[13], &pstExtra->ubaMacAddr[3], 2); 

	//* 显式地通知后续的处理函数这是链路本地地址不是根据路由器发布的前缀生成或dhcpv6服务器分配的动态地址
	pstNetif->nif_lla_ipv6[15] = IPv6LNKADDR_FLAG;

	return ubaLnkAddr; 
}

const UCHAR *ipv6_dyn_addr(PST_NETIF pstNetif, UCHAR ubaDynAddr[16], UCHAR *pubPrefix, UCHAR ubPrefixBitLen)
{
	UCHAR ubPrefixBytes = ubPrefixBitLen / 8 + (ubPrefixBitLen % 8 ? 1 : 0); 
	memcpy(ubaDynAddr, pubPrefix, ubPrefixBytes);

	//* 如果前缀长度为64位，则直接使用EUI-64编码方式生成后面的接口标识符，及动态地址为：前缀::/64 + EUI-64地址，否则随机生成接口标识符
	if (ubPrefixBitLen == 64)
	{
		PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;

		//* 复制以太网mac地址前24位
		UCHAR ubUField = pstExtra->ubaMacAddr[0];
		memcpy(&ubaDynAddr[8], pstExtra->ubaMacAddr, 3);

		//* 按照惯例在这里将mac地址U/L位取反以提供最大程度的可压缩性（mac地址U位为1时取反为0则可提高ipv6地址连续全0字段的概率，因为
		//* 协议栈运行环境基本都是自行指定mac地址，此时U位应为1以显式地告知这是本地mac地址）
		ubUField = (ubUField & 0xFD) | (~ubUField & 0x02);
		ubaDynAddr[8] = ubUField;
		ubaDynAddr[11] = 0xFF;
		ubaDynAddr[12] = 0xFE;
		memcpy(&ubaDynAddr[13], &pstExtra->ubaMacAddr[3], 2); 
	}
	else
	{
		//* 生成随机的接口标识符
		rand_any_bytes(&ubaDynAddr[ubPrefixBytes], 16 - ubPrefixBytes - 1); 
	}

	ubaDynAddr[15] = IPv6LNKADDR_FLAG + 1;

	return ubaDynAddr; 
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

static INT icmpv6_send(PST_NETIF pstNetif, UCHAR ubType, UCHAR ubCode, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR *pubData, UINT unDataLen, EN_ONPSERR *penErr)
{
	ST_ICMPv6_HDR stHdr;

	//* 申请一个buf list节点用于挂载数据
	SHORT sBufListHead = -1;	
	SHORT sDataNode = buf_list_get_ext(pubData, (UINT)unDataLen, penErr);
	if (sDataNode < 0)
		return -1;
	buf_list_put_head(&sBufListHead, sDataNode);

	//* 申请一个buf list节点挂载icmpv6头
	SHORT sIcmpv6HdrNode = buf_list_get_ext(&stHdr, (UINT)sizeof(ST_ICMPv6_HDR), penErr);
	if (sIcmpv6HdrNode < 0)
	{
		buf_list_free(sDataNode);
		return -1;
	}
	buf_list_put_head(&sBufListHead, sIcmpv6HdrNode);

	//* 封装icmpv6 ns报文
	//* ================================================================================
	stHdr.ubType = ubType;
	stHdr.ubCode = ubCode;
	stHdr.usChecksum = 0;
	//* ================================================================================

	//* 根据报文类型生成icmpv6校验和计算的源和目的地址
	//* ================================================================================
	UCHAR ubaSrcIpv6Used[16], ubaDstIpv6Used[16];
	if (ubaSrcIpv6)
		memcpy(ubaSrcIpv6Used, ubaSrcIpv6, 16);
	else
		memset(ubaSrcIpv6Used, 0, sizeof(ubaSrcIpv6Used)); 

	switch(ubType)
	{
	case ICMPv6_NS: //* Neighbor Solicitation，邻居请求，根据目的地址生成组播地址：FF02::1:FF00:0/104 + 目的地址的后24位		
		memcpy(ubaDstIpv6Used, l_ubaSolNodeMcAddrPrefix, sizeof(l_ubaSolNodeMcAddrPrefix));
		memcpy(&ubaDstIpv6Used[sizeof(l_ubaSolNodeMcAddrPrefix)], &ubaDstIpv6[sizeof(l_ubaSolNodeMcAddrPrefix)], 3);
		break; 

	default: 
		memcpy(ubaDstIpv6Used, ubaDstIpv6, 16);
		break; 
	}	
	//* ================================================================================

	//* 计算校验和
	stHdr.usChecksum = tcpip_checksum_ipv6(ubaSrcIpv6Used, ubaDstIpv6Used, (UINT)(sizeof(ST_ICMPv6_HDR) + unDataLen), IPPROTO_ICMPv6, sBufListHead, penErr);

	//* 发送，并释放占用的buf list节点	
	INT nRtnVal = ipv6_send(pstNetif, NULL, ubaSrcIpv6Used, ubaDstIpv6Used, IPPROTO_ICMPv6, sBufListHead, 0, penErr);
	buf_list_free(sDataNode);
	buf_list_free(sIcmpv6HdrNode); 

	return nRtnVal; 
}

INT icmpv6_send_ns(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], EN_ONPSERR *penErr)
{	
	PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;

	UCHAR ubaNSolicitation[sizeof(ST_ICMPv6_NS_HDR) + sizeof(ST_ICMPv6NDOPT_LLA)];
	PST_ICMPv6_NS_HDR pstNeiSolHdr = (PST_ICMPv6_NS_HDR)ubaNSolicitation; 
	PST_ICMPv6NDOPT_LLA pstOptLnkAddr = (PST_ICMPv6NDOPT_LLA)&ubaNSolicitation[sizeof(ST_ICMPv6_NS_HDR)];

	//* 封装icmpv6 neighbor solicitation报文
	//* ================================================================================	
	pstNeiSolHdr->unReserved = 0; 
	memcpy(pstNeiSolHdr->ubaTargetAddr, ubaDstIpv6, 16); 
	if (ubaSrcIpv6)
	{
		pstOptLnkAddr->stHdr.ubType = ICMPV6OPT_SLLA;
		pstOptLnkAddr->stHdr.ubLen = 1; //* 单位：8字节
		memcpy(pstOptLnkAddr->ubaAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN);
	}	
	//* ================================================================================

	//* 完成实际的发送并释放占用的buf list节点
	return icmpv6_send(pstNetif, ICMPv6_NS, 0, ubaSrcIpv6, ubaDstIpv6, ubaNSolicitation, ubaSrcIpv6 ? (UINT)sizeof(ubaNSolicitation) : (UINT)sizeof(ST_ICMPv6_NS_HDR), penErr);		
}

INT icmpv6_send_rs(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], EN_ONPSERR *penErr)
{
	PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra; 

	UCHAR ubaRSolicitation[sizeof(ST_ICMPv6_RS_HDR) + sizeof(ST_ICMPv6NDOPT_LLA)];
	PST_ICMPv6_RS_HDR pstRtrSolHdr = (PST_ICMPv6_RS_HDR)ubaRSolicitation;
	PST_ICMPv6NDOPT_LLA pstOptLnkAddr = (PST_ICMPv6NDOPT_LLA)&ubaRSolicitation[sizeof(ST_ICMPv6_RS_HDR)];

	//* 封装icmpv6 router solicitation报文
	//* ================================================================================	
	pstRtrSolHdr->unReserved = 0; 
	pstOptLnkAddr->stHdr.ubType = ICMPV6OPT_SLLA;
	pstOptLnkAddr->stHdr.ubLen = 1; //* 单位：8字节
	memcpy(pstOptLnkAddr->ubaAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN);
	//* ================================================================================
	
	//* 完成实际的发送并释放占用的buf list节点
	return icmpv6_send(pstNetif, ICMPv6_RS, 0, ubaSrcIpv6, (UCHAR *)ipv6_mc_addr(IPv6MCA_ALLROUTERS), ubaRSolicitation, sizeof(ubaRSolicitation), penErr); 
}

static void icmpv6_ns_handler(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], UCHAR *pubIcmpv6)
{
	PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
	PST_ICMPv6_NS_HDR pstNSolHdr = (PST_ICMPv6_NS_HDR)(pubIcmpv6 + sizeof(ST_ICMPv6_HDR)); 

	UCHAR ubaNAdvertisement[sizeof(ST_ICMPv6_NA_HDR) + sizeof(ST_ICMPv6NDOPT_LLA)]; 
	PST_ICMPv6_NA_HDR pstNAdvertHdr = (PST_ICMPv6_NA_HDR)ubaNAdvertisement; 
	pstNAdvertHdr->icmpv6_na_flag = 0; 
	pstNAdvertHdr->icmpv6_na_flag_s = 1; 
	pstNAdvertHdr->icmpv6_na_flag_o = 1;
	pstNAdvertHdr->icmpv6_na_flag = htonl(pstNAdvertHdr->icmpv6_na_flag);
	memcpy(pstNAdvertHdr->ubaTargetAddr, pstNSolHdr->ubaTargetAddr, 16); 

	PST_ICMPv6NDOPT_LLA pstOptLnkAddr = (PST_ICMPv6NDOPT_LLA)&ubaNAdvertisement[sizeof(ST_ICMPv6_NA_HDR)]; 
	pstOptLnkAddr->stHdr.ubType = ICMPV6OPT_TLLA;
	pstOptLnkAddr->stHdr.ubLen = 1; 
	memcpy(pstOptLnkAddr->ubaAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN); 

	icmpv6_send(pstNetif, ICMPv6_NA, 0, pstNSolHdr->ubaTargetAddr, ubaSrcIpv6, ubaNAdvertisement, sizeof(ubaNAdvertisement), NULL);
}

//* 收到的NA（Neighbor Advertisement，邻居通告）报文处理函数
static void icmpv6_na_handler(PST_NETIF pstNetif, UCHAR *pubIcmpv6)
{	
	PST_ICMPv6_NA_HDR pstNeiAdvHdr = (PST_ICMPv6_NA_HDR)(pubIcmpv6 + sizeof(ST_ICMPv6_HDR));
	PST_ICMPv6NDOPT_LLA pstOptLnkAddr = (PST_ICMPv6NDOPT_LLA)(pubIcmpv6 + sizeof(ST_ICMPv6_HDR) + sizeof(ST_ICMPv6_NA_HDR));
	pstNeiAdvHdr->icmpv6_na_flag = htonl(pstNeiAdvHdr->icmpv6_na_flag);

	//* 首先看看Solicited标志是否置位，是，意味着这是NS报文的应答报文
	if (pstNeiAdvHdr->icmpv6_na_flag_s)
	{
		//* 更新ipv6 to mac缓存映射表，注意只有主动请求的响应报文才会操作缓存映射表，这样可避免频繁的通告导致缓存表无法保持相对稳定的问题，另外只有override标志置1才会更新已存在条目，否则只增加新条目		
		ipv6_mac_add_entry(pstNetif, pstNeiAdvHdr->ubaTargetAddr, pstOptLnkAddr->ubaAddr, pstNeiAdvHdr->icmpv6_na_flag_o);
	}
	else
	{
		//* 这是一个邻居节点通告，且是面向链路上所有节点（目标地址为FF02::1），所以只要是仍在地址自动配置阶段就需要判断是否存在地址冲突，因为
		//* 存在某个节点响应很慢的情形，导致DAD检测结束才收到冲突报文，虽然概率很低，但为了可靠必须处理这种情况
		if (pstNetif->stIPv6.bitCfgState == IPv6CFG_LNKADDR)
		{
			if (pstNetif->stIPv6.stLnkAddr.bitState == IPv6ADDR_TENTATIVE 
				&& !pstNetif->stIPv6.stLnkAddr.bitConflict
				&& !memcmp(pstNeiAdvHdr->ubaTargetAddr, pstNetif->nif_lla_ipv6, 16))
				pstNetif->stIPv6.stLnkAddr.bitConflict = TRUE; 
		}
		else if(IPv6CFG_LNKADDR < pstNetif->stIPv6.bitCfgState < IPv6CFG_END)
		{
			PST_IPv6_DYNADDR pstNextAddr = NULL; 
			do {
				//* 逐个读取已挂载到网卡上的动态地址节点，只要是正处于试探阶段的地址都要判断下看是否其已经被其它节点使用
				pstNextAddr = netif_ipv6_dyn_addr_next_safe(pstNetif, pstNextAddr, TRUE);
				if (pstNextAddr && pstNextAddr->bitState == IPv6ADDR_TENTATIVE)
				{
					if (!pstNextAddr->bitConflict && !memcmp(pstNeiAdvHdr->ubaTargetAddr, pstNextAddr->ubaVal, 16))
					{
						pstNextAddr->bitConflict = TRUE; 						
						netif_ipv6_dyn_addr_release(pstNextAddr); //* 释放当前地址节点
						break; 
					}
				}
			} while (pstNextAddr); 
		}
		else; //* 其它节点地址冲突导致的全链路节点通告，不用理会
	}
}

static void icmpv6_ra_opt_prefix_info_handler(PST_NETIF pstNetif, PST_IPv6_ROUTER pstRouter, PST_ICMPv6_NDOPT_PREFIXINFO pstPrefixInfo) 
{
	if (pstPrefixInfo->stHdr.ubLen != 4)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("Prefix information option length (%d bytes) error.\r\n", pstPrefixInfo->stHdr.ubLen * 8);
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
	}

	if (pstPrefixInfo->ubPrefixBitLen > 104)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("The prefix is too long, and the maximum acceptable length for the protocol stack is 104.\r\n");
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
	}

	UINT unValidLifetime = htonl(pstPrefixInfo->unValidLifetime);
	UINT unPreferredLifetime = htonl(pstPrefixInfo->unPreferredLifetime); 

	if (unValidLifetime < unPreferredLifetime)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("The valid lifetime (%d) is less than the preferred lifetime (%d).\r\n", unValidLifetime, unPreferredLifetime);
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
		return; 
	}	

	os_critical_init();

	//* 先看看前缀长度是否过长
	if (pstPrefixInfo->ubPrefixBitLen > 128 - (ETH_MAC_ADDR_LEN * 8))
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("The prefix length %d is greater than the upper limit (%d) specified by the protocol stack.\r\n", pstPrefixInfo->ubPrefixBitLen, 128 - (ETH_MAC_ADDR_LEN * 8)); 
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
		return; 
	}

	//* 首先看看这个前缀是否已经生成地址
	PST_IPv6_DYNADDR pstAddr = NULL; 
	do {
		//* 读取地址节点
		pstAddr = netif_ipv6_dyn_addr_next_safe(pstNetif, pstAddr, TRUE); 
		if (pstAddr && pstAddr->bitPrefixBitLen)
		{
			//* 前缀长度一致，且前缀匹配，则不再生成地址
			if (pstAddr->bitPrefixBitLen == pstPrefixInfo->ubPrefixBitLen && !ipv6_addr_cmp(pstPrefixInfo->ubaPrefix, pstAddr->ubaVal, pstAddr->bitPrefixBitLen))
			{			
				//* 更新地址有效时间，这个地方按照[RFC4862] 5.5.3节P19的描述，应当按照如下规则更新：
				//* 1. 如果报文中的有效生存时间大于2小时或大于地址节点当前的剩余有效生存时间，则用报文中的有效生存时间覆盖当前剩余生存时间；
				//* 2. 如果地址节点的剩余生存期小于等于2小时则继续递减，忽略当前报文中的有效生存时间，除非RA报文已通过认证（比如SEND协议，Secure Neighbor Discovery [RFC3971]）；
				//* 3. 其它情况，重置有效生存时间为2小时；
				//* 首选生存时间直接重置为通告中的首先生存时间。
				//* 上述规则主要是避免非法RA报文携带短生存时间前缀导致拒绝服务问题的出现。
				os_enter_critical(); 
				{
					if(unValidLifetime > 7200 || unValidLifetime > pstAddr->unValidLifetime)
						pstAddr->unValidLifetime = unValidLifetime + IPv6ADDR_INVALID_TIME;
					else
					{
						if (pstAddr->unValidLifetime > 7200)
							pstAddr->unValidLifetime = 7200 + IPv6ADDR_INVALID_TIME;
					}
					
					//* 路由器管理员有可能通过很短的首选生存时间来主动弃用某个地址，所以这个选项必须无条件更新（显然相对于将很短的有效生存时间视为非法的规则，该规则重点考虑了网络管理的便利性）
					pstAddr->unPreferredLifetime = unPreferredLifetime;  
					if (unPreferredLifetime && pstAddr->bitState > IPv6ADDR_PREFERRED)
						pstAddr->bitState = IPv6ADDR_PREFERRED; //* 再次调整为地址“可用”状态
				}
				os_exit_critical(); 

				netif_ipv6_dyn_addr_release(pstAddr); //* 处理完毕释放当前地址节点，其实就是引用计数减一
				return;
			}
		}
	} while (pstAddr);

	//* 申请一个动态地址节点
	EN_ONPSERR enErr; 
	pstAddr = ipv6_dyn_addr_node_get(NULL, &enErr); 
	if (pstAddr)
	{
		//* 生成试探ipv6地址、初始地址相关控制字段并加入网卡
		ipv6_dyn_addr(pstNetif, pstAddr->ubaVal, pstPrefixInfo->ubaPrefix, pstPrefixInfo->ubPrefixBitLen);
		pstAddr->bitRouter = ipv6_router_get_index(pstRouter);
		pstAddr->bitPrefixBitLen = pstPrefixInfo->ubPrefixBitLen;
		pstAddr->unValidLifetime = unValidLifetime ? unValidLifetime + IPv6ADDR_INVALID_TIME : IPv6ADDR_INVALID_TIME + 1; 
		pstAddr->unPreferredLifetime = unPreferredLifetime; 
		pstAddr->bitState = IPv6ADDR_TENTATIVE; 
		netif_ipv6_dyn_addr_add(pstNetif, pstAddr);		

		//* 开启重复地址检测
		if (ipv6_cfg_dad(pstNetif, pstAddr, &enErr))
			return; 
	}	

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
#if PRINTF_THREAD_MUTEX
	os_thread_mutex_lock(o_hMtxPrintf);
#endif
	printf("icmpv6_ra_opt_prefix_info_handler() failed, %s\r\n", onps_error(enErr));
#if PRINTF_THREAD_MUTEX
	os_thread_mutex_unlock(o_hMtxPrintf);
#endif
#endif
}

static void icmpv6_ra_opt_rdnssrv_handler(PST_NETIF pstNetif, PST_IPv6_ROUTER pstRouter, PST_ICMPv6NDOPT_RDNSSRV_HDR pstOption)
{
	if (pstOption->stHdr.ubLen < 3)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
#endif
		printf("Recursive DNS Server Option length (%d bytes) too small\r\n", pstOption->stHdr.ubLen * 8);
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
#endif
#endif
		return; 
	}

	//* 携带的dns服务器地址数量
	CHAR i;
	CHAR bSrvNum = ((pstOption->stHdr.ubLen * 8) - 8) / 16; 	
	UCHAR *pubIpv6 = (UCHAR *)pstOption + sizeof(ST_ICMPv6NDOPT_RDNSSRV_HDR); 

	bSrvNum = bSrvNum > 2 ? 2 : bSrvNum; 
	i = bSrvNum; 

	os_critical_init(); 

	//* 只有生存期比现有生存期还大才会更新
	if (pstOption->unLifetime > pstRouter->staDNSSrv[0].unLifetime)
	{	
		//* 已经得到dns服务器地址
		if (pstRouter->staDNSSrv[0].ubaAddr[0])
		{
			//* 需要看看新的RA报文是否携带了完全不同的dns服务器地址，如果不是则仅更新生存时间即可
			for (i = 0; i < bSrvNum; i++)
			{
				if (!memcmp(pubIpv6 + i * 16, pstRouter->staDNSSrv[0].ubaAddr, 16))
				{
					os_enter_critical();
					pstRouter->staDNSSrv[0].unLifetime = pstOption->unLifetime;
					os_exit_critical();
					goto __lblSlave;
				}
			}
		}		

		os_enter_critical();
		{			
			//* 直接用第一个地址更新主dns服务器地址，同时更新生存时间
			i = 0; 
			memcpy(pstRouter->staDNSSrv[0].ubaAddr, pubIpv6, 16);
			pstRouter->staDNSSrv[0].unLifetime = pstOption->unLifetime;
		}
		os_exit_critical(); 
	}

__lblSlave: 
	//* 存在两个地址，则判断从服务器地址是否需要更新，依然是生存期比较
	if (bSrvNum == 2 && pstOption->unLifetime > pstRouter->staDNSSrv[1].unLifetime)
	{
		CHAR bAddrIdx = i;

		if (pstRouter->staDNSSrv[1].ubaAddr[0])
		{			
			for (i = 0; i < bSrvNum; i++)
			{
				if (!memcmp(pubIpv6 + i * 16, pstRouter->staDNSSrv[1].ubaAddr, 16))
				{
					//* 说明主从地址相等，那么从地址将直接清零
					if (bAddrIdx == i)
					{
						os_enter_critical();
						{
							pstRouter->staDNSSrv[1].ubaAddr[0] = 0; 
							pstRouter->staDNSSrv[1].unLifetime = 0;
						}
						os_exit_critical();
						return;
					}

					//* 匹配，则只更新生存时间
					os_enter_critical();
					pstRouter->staDNSSrv[1].unLifetime = pstOption->unLifetime;
					os_exit_critical();

					return;
				}
			}
		}		

		//* 主地址存在匹配的，需要用通告的另一个地址作为从dns服务器地址
		if (bAddrIdx < bSrvNum)
			bAddrIdx = bAddrIdx ? 0 : 1; 
		else
			bAddrIdx = 0; 

		os_enter_critical();
		{
			//* 直接用第一个地址更新主dns服务器地址，同时更新生存时间				
			memcpy(pstRouter->staDNSSrv[1].ubaAddr, pubIpv6 + bAddrIdx * 16, 16); 
			pstRouter->staDNSSrv[1].unLifetime = pstOption->unLifetime;
		}
		os_exit_critical();
	}
}

static void icmpv6_ra_option_handler(PST_NETIF pstNetif, PST_IPv6_ROUTER pstRouter, UCHAR *pubOpt, SHORT sOptLen)
{
	PST_ICMPv6NDOPT_HDR pstOptHdr = (PST_ICMPv6NDOPT_HDR)pubOpt; 
	USHORT usMtu; 
		
	while (sOptLen)
	{
		switch (pstOptHdr->ubType)
		{
		case ICMPv6NDOPT_SRCLNKADDR: 
			ipv6_mac_add_entry_ext(((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbIpv6Mac, pstRouter->ubaAddr, ((PST_ICMPv6NDOPT_LLA)pstOptHdr)->ubaAddr, TRUE);
			//memcpy(pstRouter->ubaMacAddr, ((PST_ICMPv6NDOPT_LLA)pstOptHdr)->ubaAddr, ETH_MAC_ADDR_LEN); 
			break; 

		case ICMPv6NDOPT_PREFIXINFO:
			//* 按照[rfc4862]5.5.3节给出的算法说明，A标志未置位或者前缀为链路本地地址则直接丢弃Prefix information选项
			if(((PST_ICMPv6_NDOPT_PREFIXINFO)pstOptHdr)->icmpv6ndopt_pi_flag_A 
				&& ipv6_addr_cmp(((PST_ICMPv6_NDOPT_PREFIXINFO)pstOptHdr)->ubaPrefix, l_ubaLinkLocalAddrPrefix, 64))
				icmpv6_ra_opt_prefix_info_handler(pstNetif, pstRouter, (PST_ICMPv6_NDOPT_PREFIXINFO)pstOptHdr); 
			break; 

		case ICMPv6NDOPT_MTU:
			usMtu = (USHORT)htonl(((PST_ICMPv6NDOPT_MTU)pstOptHdr)->unMtu);
			if(usMtu > 1200 && usMtu < 1500)
				pstRouter->usMtu = usMtu; 
			break; 

		case ICMPv6NDOPT_RDNSSRV: 
			icmpv6_ra_opt_rdnssrv_handler(pstNetif, pstRouter, (PST_ICMPv6NDOPT_RDNSSRV_HDR)pstOptHdr); 
			break; 

		case ICMPv6NDOPT_ROUTERINFO: //* Type B Host，忽略该选项，参见[RFC4191]第3节“Conceptual Model of a Host”：https://www.rfc-editor.org/rfc/rfc4191.html#section-3
		default: 
	#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("Unsupported RA options: %d\r\n", pstOptHdr->ubType);
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
			break; 
		}
				
		sOptLen -= ((SHORT)pstOptHdr->ubLen * 8); 
		pstOptHdr = (PST_ICMPv6NDOPT_HDR)((UCHAR *)pstOptHdr + pstOptHdr->ubLen * 8); 
	}	
}

//* 
//* 收到的RA（Router Advertisement，邻居通告）报文处理函数，算法实现依据来自[RFC4861]6.3.4节：https://www.rfc-editor.org/rfc/rfc4861#section-6.3.4
//* 关于收到的前缀信息选项如何处理，详见[RFC4862]5.5.3节：https://www.rfc-editor.org/rfc/rfc4862#section-5.5.3
static void icmpv6_ra_handler(PST_NETIF pstNetif, UCHAR ubaRouterIpv6[16], UCHAR *pubIcmpv6, USHORT usIcmpv6PktLen)
{
	PST_ICMPv6_RA_HDR pstRouterAdvHdr = (PST_ICMPv6_RA_HDR)(pubIcmpv6 + sizeof(ST_ICMPv6_HDR));
	PST_IPv6_ROUTER pstRouter; 
	EN_ONPSERR enErr; 

	os_critical_init();

	//* 先看该路由器是否已经被添加到链表中
	pstRouter = netif_ipv6_router_get_by_addr(pstNetif, ubaRouterIpv6); 
	if (pstRouter)
	{
		//* 更新相关标志
		//pstRouter->i6r_flag = (pstRouter->i6r_flag & i6r_ref_cnt_mask) | (pstRouterAdvHdr->icmpv6_ra_flag & i6r_flag_mask); 
		pstRouter->i6r_flag_prf = i6r_prf_converter((pstRouterAdvHdr->icmpv6_ra_flag_prf != 2) ? pstRouterAdvHdr->icmpv6_ra_flag_prf : 0);
	}
	else 
	{
		//* 先看看路由器生存时间是否为0.如果为零则直接忽略当前通告报文，不进行任何处理
		if (pstRouterAdvHdr->usLifetime == 0)
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("Received an RA packet with a router lifetime of 0, which will be ignored\r\n");
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif

			return;
		}

		//* 申请一个路由器节点用于保存当前路由器相关信息
		pstRouter = ipv6_router_node_get(NULL, &enErr); 
		if (!pstRouter)
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL > 0		
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("icmpv6_ra_handler() failed, %s\r\n", onps_error(enErr)); 
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif

			return; 
		}

		//* 保存路由器地址
		memcpy(pstRouter->ubaAddr, ubaRouterIpv6, 16); 
		pstRouter->i6r_flag_prf = i6r_prf_converter((pstRouterAdvHdr->icmpv6_ra_flag_prf != 2) ? pstRouterAdvHdr->icmpv6_ra_flag_prf : 0);
		pstRouter->i6r_ref_cnt = 0; 
		pstRouter->usMtu = 1492; 
		pstRouter->ubHopLimit = 255; 
		pstRouter->pstNetif = NULL;  
		pstRouter->bDv6Client = INVALID_ARRAYLNKLIST_UNIT;
		memset(&pstRouter->staDNSSrv, 0, sizeof(pstRouter->staDNSSrv)); 		
	}

	//* 赋值
	//* =================================================================
	pstRouter->ubHopLimit = pstRouterAdvHdr->ubHopLimit ? pstRouterAdvHdr->ubHopLimit : pstRouter->ubHopLimit; //* 非零值才会更新该字段，否则保留原值

	os_enter_critical();
	pstRouter->usLifetime = htons(pstRouterAdvHdr->usLifetime); 
	os_exit_critical(); 	
	//* =================================================================
	
	
	UCHAR ubHdrLen = sizeof(ST_ICMPv6_HDR) + sizeof(ST_ICMPv6_RA_HDR);
	//* 不为NULL说明这是已经添加到网卡中的路由器节点
	if (pstRouter->pstNetif)
	{
		//* 处理携带的icmpv6选项		
		icmpv6_ra_option_handler(pstNetif, pstRouter, pubIcmpv6 + ubHdrLen, (SHORT)(usIcmpv6PktLen - ubHdrLen));

		//* 释放占用的路由器（取消删除保护）
		netif_ipv6_router_release(pstRouter); 
	}
	else
	{
		//* 处理携带的icmpv6选项之前先赋值ST_IPv6_ROUTER::pstNetif字段，后续DAD检测需要用到（如果存在前缀或路由器信息选项且可以进行无状态地址配置）
		pstRouter->pstNetif = pstNetif; 						
		icmpv6_ra_option_handler(pstNetif, pstRouter, pubIcmpv6 + ubHdrLen, (SHORT)(usIcmpv6PktLen - ubHdrLen));		

		//* 添加到网卡		
		netif_ipv6_router_add(pstNetif, pstRouter);				
	}	

	//* 根据m和o标志确定是否发送dhcpv6请求包，参见[RFC4861]4.2节：https://www.rfc-editor.org/rfc/rfc4861#section-4.2
	if (pstRouterAdvHdr->icmpv6_ra_flag_m || pstRouterAdvHdr->icmpv6_ra_flag_o)
	{
		//* 开启DHCPv6配置请求流程
		pstRouter->bitDv6CfgState = Dv6CFG_START;
		pstRouter->i6r_flag_m = pstRouterAdvHdr->icmpv6_ra_flag_m;
		if (dhcpv6_client_start(pstRouter, &enErr) < 0)
		{
			pstRouter->bitDv6CfgState = Dv6CFG_END;

	#if SUPPORT_PRINTF && DEBUG_LEVEL > 0		
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("dhcpv6_client_start() failed, %s\r\n", onps_error(enErr));
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
		}
	}
	else
		pstRouter->bitDv6CfgState = Dv6CFG_END;
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
		pstIcmpv6Hdr->usChecksum = usPktChecksum;
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
		icmpv6_ns_handler(pstNetif, pstIpv6Hdr->ubaSrcIpv6, pubIcmpv6);
		break; 

	case ICMPv6_NA:
		icmpv6_na_handler(pstNetif, pubIcmpv6);
		break; 

	case ICMPv6_RA:
		icmpv6_ra_handler(pstNetif, pstIpv6Hdr->ubaSrcIpv6, pubIcmpv6, usIpv6PayloadLen);
		break; 

	default: 
		printf("++++++++recv %d packet\r\n", pstIcmpv6Hdr->ubType);
		break; 
	}
}
#endif

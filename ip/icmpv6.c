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
#include "onps_utils.h"
#include "onps_input.h"
#include "netif/netif.h"

#if SUPPORT_IPV6
#define SYMBOL_GLOBALS
#include "ip/icmpv6.h"
#undef SYMBOL_GLOBALS

#if SUPPORT_ETHERNET
 //* Ipv6到以太网Mac地址映射表，该表仅缓存最近通讯的主机映射，缓存数量由sys_config.h中IPV6TOMAC_ENTRY_NUM宏指定
static STCB_ETHIPv6MAC l_stcbaEthIpv6Mac[ETHERNET_NUM];

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

void ipv6_mac_add_entry(PST_NETIF pstNetif, UCHAR ubaIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN])
{
	BOOL blIsExist;
	PSTCB_ETHIPv6MAC pstcbIpv6Mac = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbIpv6Mac; 

	os_critical_init();

	//* 先查找该条目是否已经存在，如果存在则直接更新，否则直接添加或替换最老条目
	INT i;
	for (i = 0; i < pstcbIpv6Mac->bEntriesNum/*IPV6TOMAC_ENTRY_NUM*/; i++) 
	{			
		if (!ipv6_addr_cmp(ubaIpv6, pstcbIpv6Mac->staEntry[i].ubaIpv6, 128)) //* 匹配
		{
			//* 更新mac地址
			os_enter_critical();
			{
				memcpy(pstcbIpv6Mac->staEntry[i].ubaMac, ubaMacAddr, ETH_MAC_ADDR_LEN); 
				pstcbIpv6Mac->staEntry[i].unUpdateTime = os_get_system_secs();
			}
			os_exit_critical();
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
		blIsExist = FALSE;

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

void ipv6_mac_add_entry_ext(PSTCB_ETHIPv6MAC pstcbIpv6Mac, UCHAR ubaIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN])
{
	os_critical_init();

	//* 先查找该条目是否已经存在，如果存在则直接更新，否则直接添加或替换最老条目
	INT i;
	for (i = 0; i < pstcbIpv6Mac->bEntriesNum; i++)
	{		
		if (!ipv6_addr_cmp(ubaIpv6, pstcbIpv6Mac->staEntry[i].ubaIpv6, 128)) //* 匹配
		{
			//* 更新mac地址
			os_enter_critical();
			{
				memcpy(pstcbIpv6Mac->staEntry[i].ubaMac, ubaMacAddr, ETH_MAC_ADDR_LEN);
				pstcbIpv6Mac->staEntry[i].unUpdateTime = os_get_system_secs();
			}
			os_exit_critical();
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
#endif

void icmpv6_start_config(PST_NETIF pstNetif, EN_ONPSERR *penErr)
{

}
#endif
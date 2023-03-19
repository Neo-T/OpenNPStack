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

void ipv6_to_mac_mapping_tbl_init(void)
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

PSTCB_ETHIPv6MAC ipv6_to_mac_ctl_block_new(void)
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
				break;
			}
		}
	}
	os_exit_critical();

	return pstcbIpv6Mac;
}

void ipv6_to_mac_ctl_block_free(PSTCB_ETHIPv6MAC pstcbIpv6Mac)
{
	memset(pstcbIpv6Mac, 0, sizeof(STCB_ETHIPv6MAC));
	pstcbIpv6Mac->bIsUsed = FALSE; 
}
#endif

void icmpv6_start_config(PST_NETIF pstNetif, EN_ONPSERR *penErr)
{

}
#endif
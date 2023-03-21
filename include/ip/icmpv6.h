/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * ICMPv6协议相关功能函数
 *
 * Neo-T, 创建于2023.03.12 09:33
 *
 */
#ifndef ICMPv6_H
#define ICMPv6_H

#ifdef SYMBOL_GLOBALS
	#define ICMPv6_EXT
#else
	#define ICMPv6_EXT extern
#endif //* SYMBOL_GLOBALS
#include "icmpv6_frame.h"

#if SUPPORT_IPV6
typedef struct _ST_NETIF_ ST_NETIF, *PST_NETIF; 
typedef struct _ST_ONESHOTTIMER_ ST_ONESHOTTIMER, *PST_ONESHOTTIMER; 

#if SUPPORT_ETHERNET
//* IPv6地址到以太网Mac地址映射表存储结构体
typedef struct _ST_ENTRY_ETHIPv6MAC_ {
	UINT unUpdateTime;	//* 条目更新（读取/缓存）时间
	UCHAR ubaIpv6[16];	//* IPv6地址
	UCHAR ubaMac[ETH_MAC_ADDR_LEN]; //* 对应的ip地址    
} ST_ENTRY_ETHIPv6MAC, *PST_ENTRY_ETHIPv6MAC;

//* Ipv6到以太网Mac地址映射表控制块
typedef struct _STCB_ETHIPv6MAC__ {
	CHAR bIsUsed;	
	CHAR bLastReadEntryIdx; //* 最近读取的映射条目
	CHAR bEntriesNum;		//* 已经缓存的条目数量
	ST_ENTRY_ETHIPv6MAC staEntry[IPV6TOMAC_ENTRY_NUM]; //* IPv6地址到以太网Mac地址映射表

	ST_SLINKEDLIST_NODE staSListWaitQueue[12]; //* 等待icmpv6查询结果的待发送报文队列    
	PST_SLINKEDLIST pstSListWaitQueueFreed;
	PST_SLINKEDLIST pstSListWaitQueue;
} STCB_ETHIPv6MAC, *PSTCB_ETHIPv6MAC;

//* ipv6报文待发送队列控制块（触发发送的依据是收到邻居节点地址请求（Neighbor Solicitation）报文的应答报文）
typedef struct _STCB_ETHIPv6MAC_WAIT_ {
	PST_ONESHOTTIMER pstTimer;
	PST_NETIF pstNetif;
	PST_SLINKEDLIST_NODE pstNode;
	UCHAR ubaIpv6[16];
	USHORT usIpPacketLen;
	UCHAR ubCount;
	UCHAR ubSndStatus; 
} STCB_ETHIPv6MAC_WAIT, *PSTCB_ETHIPv6MAC_WAIT;
#endif

#if SUPPORT_ETHERNET
ICMPv6_EXT void ipv6_mac_mapping_tbl_init(void); 
ICMPv6_EXT PSTCB_ETHIPv6MAC ipv6_mac_ctl_block_new(void); 
ICMPv6_EXT void ipv6_mac_ctl_block_free(PSTCB_ETHIPv6MAC pstcbIpv6Mac); 
ICMPv6_EXT void ipv6_mac_add_entry(PST_NETIF pstNetif, UCHAR ubaIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN]); 
ICMPv6_EXT void ipv6_mac_add_entry_ext(PSTCB_ETHIPv6MAC pstcbIpv6Mac, UCHAR ubaIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN]);
#endif

ICMPv6_EXT void icmpv6_start_config(PST_NETIF pstNetif, EN_ONPSERR *penErr);
#endif

#endif

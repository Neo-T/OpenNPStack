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
typedef struct _ST_ENTRY_ETHIIIPV6_ {
	UINT unUpdateTime;      //* 条目更新（读取/缓存）时间
	UCHAR ubaIPv6Addr[16];	//* IPv6地址
	UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN]; //* 对应的ip地址    
} ST_ENTRY_ETHIIIPV6, *PST_ENTRY_ETHIIIPV6;

//* Ipv6到以太网Mac地址映射表控制块
typedef struct _STCB_ETHIPv6MAC__ {
	CHAR bIsUsed;	
	CHAR bLastReadEntryIdx; //* 最近读取的映射条目
	ST_ENTRY_ETHIIIPV6 staEntry[IPV6TOMAC_ENTRY_NUM]; //* IPv6地址到以太网Mac地址映射表

	ST_SLINKEDLIST_NODE staSListWaitQueue[12]; //* 等待icmpv6查询结果的待发送报文队列    
	PST_SLINKEDLIST pstSListWaitQueueFreed;
	PST_SLINKEDLIST pstSListWaitQueue;
} STCB_ETHIPv6MAC, *PSTCB_ETHIPv6MAC;

//* 等待arp查询结束后重新发送ip报文的控制块
typedef struct _STCB_ETH_ARP_WAIT_ {
	PST_ONESHOTTIMER pstTimer;
	PST_NETIF pstNetif;
	PST_SLINKEDLIST_NODE pstNode;
	UCHAR ubaIpv6[16];
	USHORT usIpPacketLen;
	UCHAR ubCount;
} STCB_ETH_ARP_WAIT, *PSTCB_ETH_ARP_WAIT;
#endif

#if SUPPORT_ETHERNET
ICMPv6_EXT void ipv6_to_mac_mapping_tbl_init(void); 
ICMPv6_EXT PSTCB_ETHIPv6MAC ipv6_to_mac_ctl_block_new(void); 
ICMPv6_EXT void ipv6_to_mac_ctl_block_free(PSTCB_ETHIPv6MAC pstcbIpv6Mac);
#endif

ICMPv6_EXT void icmpv6_start_config(PST_NETIF pstNetif, EN_ONPSERR *penErr);
#endif

#endif

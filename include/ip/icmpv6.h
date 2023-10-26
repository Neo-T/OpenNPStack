/*
 * 遵循Apache License 2.0开源许可协议
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
//#define IPv6LLA_PREFIXBITLEN	64	//* 链路本地地址前缀位长度
#define IPv6MCA_SOLPREFIXBITLEN 104	//* FF02::1:FF00:0/104，链路本地范围内的邻居节点请求组播地址前缀长度，单位；数据位

//* 协议栈支持的组播地址类型，其直接决定了icmpv6.c中l_pubaMcAddr数组单元的数据存储顺序
typedef enum {
	IPv6MCA_NETIFNODES = 0,			//* FF01::1，接口本地范围内所有节点组播地址
	IPv6MCA_ALLNODES = 1,			//* FF02::1，链路本地范围内所有节点组播地址
	IPv6MCA_SOLNODE = 2,			//* FF02::1:FF00:0/104，链路本地范围内的邻居节点请求组播地址前缀
	IPv6MCA_ALLROUTERS = 3,			//* FF02::2，链路本地范围内所有路由器组播地址
	IPv6MCA_ALLMLDv2ROUTERS = 4,	//* FF02::16，所有支持MLDv2路由器的组播地址
} EN_IPv6MCADDR_TYPE;

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
ICMPv6_EXT void ipv6_mac_add_entry(PST_NETIF pstNetif, UCHAR ubaIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], BOOL blIsOverride); 
ICMPv6_EXT void ipv6_mac_add_entry_ext(PSTCB_ETHIPv6MAC pstcbIpv6Mac, UCHAR ubaIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], BOOL blIsOverride);
ICMPv6_EXT INT ipv6_mac_get(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], EN_ONPSERR *penErr); 
ICMPv6_EXT INT ipv6_mac_get_ext(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], SHORT sBufListHead, BOOL *pblNetifFreedEn, EN_ONPSERR *penErr); 
ICMPv6_EXT const UCHAR *ipv6_lnk_addr(PST_NETIF pstNetif, UCHAR ubaLnkAddr[16]);
ICMPv6_EXT const UCHAR *ipv6_dyn_addr(PST_NETIF pstNetif, UCHAR ubaDynAddr[16], UCHAR *pubPrefix, UCHAR ubPrefixBitLen); 
ICMPv6_EXT void icmpv6_send_dst_unreachable(PST_NETIF pstNetif, UCHAR ubaDstIpv6[16], UCHAR *pubIpPacket, USHORT usIpPacketLen);
#endif

ICMPv6_EXT const CHAR *icmpv6_get_description(UCHAR ubType, UCHAR ubCode);

ICMPv6_EXT const UCHAR *ipv6_mc_addr(EN_IPv6MCADDR_TYPE enType);
ICMPv6_EXT UCHAR *ipv6_sol_mc_addr(UCHAR ubaUniIpv6[16], UCHAR ubaSolMcAddr[16]);

ICMPv6_EXT INT icmpv6_send_echo_request(INT nInput, UCHAR ubaDstIpv6[16], USHORT usIdentifier, USHORT usSeqNum, const UCHAR *pubEchoData, USHORT usEchoDataLen, EN_ONPSERR *penErr);
ICMPv6_EXT INT icmpv6_send_ns(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], EN_ONPSERR *penErr); 
ICMPv6_EXT INT icmpv6_send_rs(PST_NETIF pstNetif, UCHAR ubaSrcIpv6[16], EN_ONPSERR *penErr);
ICMPv6_EXT UCHAR *icmpv6_recv(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR *pubPacket, INT nPacketLen, UCHAR *pubIcmpv6); 
#endif

#endif

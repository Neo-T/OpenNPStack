/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 完成ethernet模块相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.06.14 17:04
 *
 */
#ifndef ETHERNET_H
#define ETHERNET_H

#ifdef SYMBOL_GLOBALS
	#define ETHERNET_EXT
#else
	#define ETHERNET_EXT extern
#endif //* SYMBOL_GLOBALS

#define LPPROTO_IP   0x00000002 //* 协议栈环回模块支持的协议：Ipv4
#define LPPROTO_IPv6 0x00000018 //* 协议栈环回模块支持的协议：Ipv6

#if SUPPORT_ETHERNET
ETHERNET_EXT void ethernet_init(void); 
ETHERNET_EXT PST_NETIF ethernet_add(const CHAR *pszIfName, const UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], PST_IPV4 pstIPv4, PFUN_EMAC_SEND pfunEmacSend, void (*pfunStartTHEmacRecv)(void *pvParam), PST_NETIF *ppstNetif, EN_ONPSERR *penErr);
ETHERNET_EXT void ethernet_del(PST_NETIF *ppstNetif); 
ETHERNET_EXT INT ethernet_ii_send(PST_NETIF pstNetif, UCHAR ubProtocol, SHORT sBufListHead, void *pvExtraParam, EN_ONPSERR *penErr);
ETHERNET_EXT void ethernet_ii_recv(PST_NETIF pstNetif, UCHAR *pubPacket, INT nPacketLen);
ETHERNET_EXT void thread_ethernet_ii_recv(void *pvParam); 
ETHERNET_EXT BOOL ethernet_ipv4_addr_matched(PST_NETIF pstNetif, in_addr_t unTargetIpAddr);
#if SUPPORT_IPV6
ETHERNET_EXT BOOL ethernet_ipv6_addr_matched(PST_NETIF pstNetif, UCHAR ubaTargetIpv6[16]);
#endif
ETHERNET_EXT void ethernet_put_packet(PST_NETIF pstNetif, PST_SLINKEDLIST_NODE pstNode); 
ETHERNET_EXT INT ethernet_loopback_put_packet(PST_NETIF pstNetif, SHORT sBufListHead, UINT unLoopProtocol);

#endif

#endif

/* ethernet.h
 *
 * 完成ethernet模块相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.06.14 17:04
 * 版本: 1.0
 *
 */
#ifndef ETHERNET_H
#define ETHERNET_H

#ifdef SYMBOL_GLOBALS
	#define ETHERNET_EXT
#else
	#define ETHERNET_EXT extern
#endif //* SYMBOL_GLOBALS

ETHERNET_EXT void ethernet_init(void); 
ETHERNET_EXT PST_NETIF ethernet_add(const CHAR *pszIfName, const UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], PST_IPV4 pstIPv4, PFUN_EMAC_SEND pfunEmacSend, void (*pfunStartTHEmacRecv)(void *pvParam), PST_NETIF *ppstNetif, EN_ONPSERR *penErr);
ETHERNET_EXT void ethernet_del(PST_NETIF *ppstNetif); 
ETHERNET_EXT INT ethernet_ii_send(PST_NETIF pstNetif, UCHAR ubProtocol, SHORT sBufListHead, void *pvExtraParam, EN_ONPSERR *penErr);
ETHERNET_EXT void ethernet_ii_recv(PST_NETIF pstNetif, UCHAR *pubPacket, INT nPacketLen);
ETHERNET_EXT void thread_ethernet_ii_recv(void *pvParam); 
ETHERNET_EXT BOOL ethernet_ipv4_addr_matched(PST_NETIF pstNetif, in_addr_t unTargetIpAddr);

#endif

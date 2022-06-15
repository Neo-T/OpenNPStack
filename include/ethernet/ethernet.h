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
ETHERNET_EXT PST_NETIF ethernet_add(const CHAR *pszIfName, const UCHAR ubaMacAddr[6], PST_IPV4 pstIPv4, PFUN_NETIF_SEND pfunSend, EN_ONPSERR *penErr);
ETHERNET_EXT void ethernet_del(PST_NETIF pstNetif); 

#endif

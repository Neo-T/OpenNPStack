/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 完成ip协议层业务逻辑实现相关的接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.04.12 10:20
 *
 */
#ifndef IP_H
#define IP_H

#ifdef SYMBOL_GLOBALS
	#define IP_EXT
#else
	#define IP_EXT extern
#endif //* SYMBOL_GLOBALS
#include "protocols.h"
#include "ip_frame.h"

typedef struct _ST_NETIF_ ST_NETIF, *PST_NETIF; 
IP_EXT INT ip_send(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, in_addr_t unSrcAddr, in_addr_t unDstAddr, EN_NPSPROTOCOL enProtocol, UCHAR ubTTL, SHORT sBufListHead, EN_ONPSERR *penErr);
IP_EXT INT ip_send_ext(in_addr_t unSrcAddr, in_addr_t unDstAddr, EN_NPSPROTOCOL enProtocol, UCHAR ubTTL, SHORT sBufListHead, EN_ONPSERR *penErr);
IP_EXT void ip_recv(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR *pubPacket, INT nPacketLen); 

#if SUPPORT_ETHERNET
IP_EXT void eth_arp_wait_timeout_handler(void *pvParam); 
#endif

#endif

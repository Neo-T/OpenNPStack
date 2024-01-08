/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.04.12 10:20
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 完成ip协议层业务逻辑实现相关的接口函数、结构体定义等工作
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

#if SUPPORT_IPV6
IP_EXT INT ipv6_send(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR ubNextHeader, SHORT sBufListHead, UINT unFlowLabel, EN_ONPSERR *penErr);
IP_EXT INT ipv6_send_ext(UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR ubNextHeader, SHORT sBufListHead, UINT unFlowLabel, EN_ONPSERR *penErr);
IP_EXT void ipv6_recv(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR *pubPacket, INT nPacketLen); 
IP_EXT UINT ipv6_flow_label_cal(UCHAR ubaDstAddr[16], UCHAR ubaSrcAddr[16], UCHAR ubNextHeader, USHORT usDstPort, USHORT usSrcPort); 
#endif

#endif

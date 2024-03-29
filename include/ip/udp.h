/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.06.01 15:29
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * udp协议相关功能函数
 *
 */
#ifndef UDP_H
#define UDP_H

#ifdef SYMBOL_GLOBALS
	#define UDP_EXT
#else
	#define UDP_EXT extern
#endif //* SYMBOL_GLOBALS
#include "ip/udp_link.h"

//* 实现udp发送，使用该函数之前用户应该已经通过connect()函数绑定了目标服务器地址
UDP_EXT INT udp_send(INT nInput, UCHAR *pubData, INT nDataLen); 
UDP_EXT INT udp_send_ext(INT nInput, SHORT sBufListHead, in_addr_t unDstIp, USHORT usDstPort, in_addr_t unSrcIp, PST_NETIF pstNetif, EN_ONPSERR *penErr);
UDP_EXT INT udp_sendto(INT nInput, in_addr_t unDstIP, USHORT usDstPort, UCHAR *pubData, INT nDataLen); 
UDP_EXT void udp_recv(in_addr_t unSrcAddr, in_addr_t unDstAddr, UCHAR *pubPacket, INT nPacketLen); 
UDP_EXT INT udp_recv_upper(INT nInput, UCHAR *pubDataBuf, UINT unDataBufSize, void *pvFromIP, USHORT *pusFromPort, CHAR bRcvTimeout);

#if SUPPORT_IPV6
UDP_EXT INT ipv6_udp_send_ext(INT nInput, SHORT sBufListHead, UCHAR ubaDstAddr[16], USHORT usDstPort, UCHAR ubaSrcAddr[16], PST_NETIF pstNetif, EN_ONPSERR *penErr);
UDP_EXT INT ipv6_udp_sendto(INT nInput, const UCHAR ubaDstAddr[16], USHORT usDstPort, UCHAR *pubData, INT nDataLen);
UDP_EXT void ipv6_udp_recv(PST_NETIF pstNetif, UCHAR ubaSrcAddr[16], UCHAR ubaDstAddr[16], UCHAR *pubPacket, INT nPacketLen);
#endif

#endif

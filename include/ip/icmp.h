/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.04.12 14:10
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * icmp协议相关功能函数
 *
 */
#ifndef ICMP_H
#define ICMP_H

#ifdef SYMBOL_GLOBALS
	#define ICMP_EXT
#else
	#define ICMP_EXT extern
#endif //* SYMBOL_GLOBALS
#include "icmp_frame.h"

typedef struct _ST_ICMP_REPORT_RESULT_ { //* 对端通过icmp报文送达的相关发送结果
    UCHAR ubType; 
    UCHAR ubCode; 
    UCHAR ubProtocol; 
    UINT unSrcAddr; 
    UINT unDstAddr; 
} ST_ICMP_REPORT_RESULT, *PST_ICMP_REPORT_RESULT;

typedef struct _ST_NETIF_ ST_NETIF, *PST_NETIF;
#if SUPPORT_ETHERNET
ICMP_EXT void icmp_send_dst_unreachable(PST_NETIF pstNetif, in_addr_t unDstAddr, UCHAR *pubIpPacket, USHORT usIpPacketLen);
#endif
ICMP_EXT INT icmp_send_echo_reqest(INT nInput, USHORT usIdentifier, USHORT usSeqNum, UCHAR ubTTL, in_addr_t unDstAddr, const UCHAR *pubData, UINT unDataSize, EN_ONPSERR *penErr);
ICMP_EXT void icmp_recv(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR *pubPacket, INT nPacketLen); //* 接收函数
ICMP_EXT void icmp_get_last_report(PST_ICMP_REPORT_RESULT pstResult);
ICMP_EXT const CHAR *icmp_get_description(UCHAR ubType, UCHAR ubCode);


#endif

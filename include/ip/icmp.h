/* icmp.h
 *
 * icmp协议相关功能函数
 *
 * Neo-T, 创建于2022.04.12 14:10
 * 版本: 1.0
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

ICMP_EXT INT icmp_send_echo_reqest(INT nInput, USHORT usIdentifier, USHORT usSeqNum, UCHAR ubTTL, UINT unDstAddr, UCHAR *pubData, UINT unDataSize, EN_ONPSERR *penErr);
ICMP_EXT void icmp_recv(UCHAR *pubPacket, INT nPacketLen); //* 接收函数


#endif

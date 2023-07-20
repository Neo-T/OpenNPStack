/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * udp协议相关功能函数
 *
 * Neo-T, 创建于2022.06.01 15:29
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

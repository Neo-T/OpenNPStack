/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * tcp协议相关功能函数
 *
 * Neo-T, 创建于2022.04.25 15:13
 *
 */
#ifndef TCP_H
#define TCP_H

#ifdef SYMBOL_GLOBALS
	#define TCP_EXT
#else
	#define TCP_EXT extern
#endif //* SYMBOL_GLOBALS
#include "ip/tcp_link.h"

TCP_EXT INT tcp_send_syn(INT nInput, in_addr_t unSrvAddr, USHORT usSrvPort, int nConnTimeout);
TCP_EXT INT tcp_send_data(INT nInput, UCHAR *pubData, INT nDataLen, int nWaitAckTimeout);
#if SUPPORT_SACK
TCP_EXT INT tcp_send_data_ext(INT nInput, UCHAR *pubData, INT nDataLen, UINT unSeqNum);
#endif
TCP_EXT void tcp_send_ack(PST_TCPLINK pstLink, in_addr_t unSrcAddr, USHORT usSrcPort, in_addr_t unDstAddr, USHORT usDstPort);
TCP_EXT void tcp_disconnect(INT nInput);
TCP_EXT void tcp_recv(void *pvSrcAddr, void *pvDstAddr, UCHAR *pubPacket, INT nPacketLen, EN_NPSPROTOCOL enProtocol);
TCP_EXT INT tcp_recv_upper(INT nInput, UCHAR *pubDataBuf, UINT unDataBufSize, CHAR bRcvTimeout); 

#if SUPPORT_SACK
TCP_EXT void thread_tcp_handler(void *pvParam); 
#endif

#if SUPPORT_IPV6
TCP_EXT INT tcpv6_send_syn(INT nInput, UCHAR ubaSrvAddr[16], USHORT usSrvPort, int nConnTimeout); 
TCP_EXT void tcpv6_send_ack(PST_TCPLINK pstLink, UCHAR ubaSrcAddr[16], USHORT usSrcPort, UCHAR ubaDstAddr[16], USHORT usDstPort); 
#endif


#endif

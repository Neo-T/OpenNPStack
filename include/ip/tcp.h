/* tcp.h
 *
 * tcp协议相关功能函数
 *
 * Neo-T, 创建于2022.04.25 15:13
 * 版本: 1.0
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
TCP_EXT void tcp_disconnect(INT nInput);
TCP_EXT void tcp_recv(in_addr_t unSrcAddr, in_addr_t unDstAddr, UCHAR *pubPacket, INT nPacketLen); 
TCP_EXT INT tcp_recv_upper(INT nInput, UCHAR *pubDataBuf, UINT unDataBufSize, CHAR bRcvTimeout);


#endif

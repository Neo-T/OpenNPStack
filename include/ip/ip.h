/* ip.h
 *
 * 完成ip协议层业务逻辑实现相关的接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.04.12 10:20
 * 版本: 1.0
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

IP_EXT INT ip_send(in_addr_t unDstAddr, EN_NPSPROTOCOL enProtocol, UCHAR ubTTL, SHORT sBufListHead, EN_ONPSERR *penErr);
IP_EXT INT ip_send_ext(in_addr_t unSrcAddr, in_addr_t unDstAddr, EN_NPSPROTOCOL enProtocol, UCHAR ubTTL, SHORT sBufListHead, EN_ONPSERR *penErr);
IP_EXT void ip_recv(UCHAR *pubPacket, INT nPacketLen);

#endif

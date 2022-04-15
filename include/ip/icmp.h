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

typedef enum  EN_ICMPTYPE;

ICMP_EXT INT icmp_send(UINT unDstAddr, EN_ICMPTYPE enType, UCHAR ubCode, const UCHAR *pubData, UINT unDataSize, EN_ERROR_CODE *penErrCode); //* 发送函数
ICMP_EXT INT icmp_recv(INT nInput, UCHAR *pubRcvBuf, UINT unRcvBufSzie, EN_ERROR_CODE *penErrCode); //* 接收函数，与发送不同的是这个函数需要传入接收控制块的句柄（入口参数nInput）


#endif

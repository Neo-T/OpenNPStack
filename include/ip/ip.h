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

typedef enum EN_IPPROTO; //* IP层支持的上层协议定义

IP_EXT INT ip_send(UINT unDstAddr, EN_IPPROTO enProto, UCHAR ubTTL, SHORT sBufListHead, EN_ERROR_CODE *penErrCode);

#endif

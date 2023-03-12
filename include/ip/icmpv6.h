/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * ICMPv6协议相关功能函数
 *
 * Neo-T, 创建于2023.03.12 09:33
 *
 */
#ifndef ICMPv6_H
#define ICMPv6_H

#ifdef SYMBOL_GLOBALS
	#define ICMPv6_EXT
#else
	#define ICMPv6_EXT extern
#endif //* SYMBOL_GLOBALS
#include "icmpv6_frame.h"

typedef struct _ST_NETIF_ ST_NETIF, *PST_NETIF; 

#endif

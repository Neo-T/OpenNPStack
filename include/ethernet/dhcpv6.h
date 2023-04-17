/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * DHCPv6协议相关功能函数
 *
 * Neo-T, 创建于2023.04.14 20:34
 *
 */
#ifndef DHCPv6_H
#define DHCPv6_H

#ifdef SYMBOL_GLOBALS
	#define DHCPv6_EXT
#else
	#define DHCPv6_EXT extern
#endif //* SYMBOL_GLOBALS
#include "dhcpv6_frame.h"

#if SUPPORT_IPV6 && SUPPORT_ETHERNET
DHCPv6_EXT INT dhcpv6_client_start(PST_IPv6_ROUTER pstRouter, EN_ONPSERR *penErr); 
#endif
#endif

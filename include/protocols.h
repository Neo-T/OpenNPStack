/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.03.26 10:00
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 协议栈支持的通讯协议类型值
 *
 */
#ifndef PROTOCOLS_H
#define PROTOCOLS_H

//* 协议栈支持的通讯协议类型值，其用于业务逻辑实现
typedef enum {
	LCP = 0, 
	PAP, 
	CHAP, 
	IPCP, 
#if SUPPORT_IPV6
	IPV6CP, 
#endif
	IPV4, 
#if SUPPORT_IPV6
	IPV6, 
#endif

	ICMP, 
#if SUPPORT_IPV6
	ICMPV6, 
#endif
	ARP, 
	TCP, 
	UDP, 
} EN_NPSPROTOCOL; 

#endif

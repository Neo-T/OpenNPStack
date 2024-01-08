/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.07.26 14:03
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 使用one-shot-timer完成dhcp客户端模块相关宏定义、接口函数、结构体定义等工作
 *
 */
#ifndef DHCP_CLIENT_BY_TIMER_H
#define DHCP_CLIENT_BY_TIMER_H

#ifdef SYMBOL_GLOBALS
	#define DHCP_CLIENT_BY_TIMER_EXT
#else
	#define DHCP_CLIENT_BY_TIMER_EXT extern
#endif //* SYMBOL_GLOBALS

#if SUPPORT_ETHERNET
DHCP_CLIENT_BY_TIMER_EXT void dhcp_req_addr_timeout_handler(void *pvParam); 
#endif

#endif

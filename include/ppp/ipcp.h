/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.04.03 14:01
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 完成ipcp协议相关基础数据结构、宏定义，功能函数声明等相关工作
 *
 */
#ifndef IPCP_H
#define IPCP_H

#ifdef SYMBOL_GLOBALS
	#define IPCP_EXT
#else
	#define IPCP_EXT extern
#endif //* SYMBOL_GLOBALS

IPCP_EXT BOOL ipcp_send_conf_request(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr);
IPCP_EXT void ipcp_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);

#endif

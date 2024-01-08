/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.04.02 19:05
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 完成pap协议相关基础数据结构、宏定义，功能函数声明等相关工作
 *
 */
#ifndef PAP_H
#define PAP_H

#ifdef SYMBOL_GLOBALS
	#define PAP_EXT
#else
	#define PAP_EXT extern
#endif //* SYMBOL_GLOBALS

PAP_EXT BOOL pap_send_auth_request(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr);
PAP_EXT void pap_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);

#endif

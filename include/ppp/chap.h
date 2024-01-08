/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.04.01 10:58
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 完成chap协议相关基础数据结构、宏定义，功能函数声明等相关工作
 *
 */
#ifndef CHAP_H
#define CHAP_H

#ifdef SYMBOL_GLOBALS
	#define CHAP_EXT
#else
	#define CHAP_EXT extern
#endif //* SYMBOL_GLOBALS

CHAP_EXT void chap_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);

#endif

/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.03.24 14:48
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 完成ppp链路的协商工作，为ip层通讯准备好ppp链路
 *
 */
#ifndef NEGOTIATION_H
#define NEGOTIATION_H

#ifdef SYMBOL_GLOBALS
	#define NEGOTIATION_EXT
#else
	#define NEGOTIATION_EXT extern
#endif //* SYMBOL_GLOBALS
#include "tty.h"
#include "negotiation_storage.h"

//* 与移动运营商协商建立ppp链路，参数pstcbPPP指向ppp链路控制块，用于保存当前ppp链路的协商状态、所使用的tty终端
//* 的句柄等信息，参数pblIsRunning则用于确保上层调用者可以随时终止当前协商过程
NEGOTIATION_EXT void ppp_link_establish(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr);
BOOL send_nego_packet(PSTCB_PPP pstcbPPP, USHORT usProtocol, UCHAR ubCode, UCHAR ubIdentifier, UCHAR *pubData, USHORT usDataLen, BOOL blIsWaitACK, EN_ONPSERR *penErr);

#endif

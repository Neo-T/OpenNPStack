/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.03.22 10:17
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 实现对modem的相关工作状态的检测、迁移等相关操作，比如确定SIM卡是否存在、拨号等操作
 *
 */
#ifndef CHAT_H
#define CHAT_H

#ifdef SYMBOL_GLOBALS
	#define CHAT_EXT
#else
	#define CHAT_EXT extern
#endif //* SYMBOL_GLOBALS

CHAT_EXT BOOL modem_ready(HTTY hTTY, EN_ONPSERR *penErr); 
CHAT_EXT BOOL modem_dial(HTTY hTTY, EN_ONPSERR *penErr);

#endif

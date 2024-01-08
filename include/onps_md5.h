/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.03.21 16:48
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 提供md5摘要计算相关的功能函数
 *
 */
#ifndef ONPS_MD5_H
#define ONPS_MD5_H

#ifdef SYMBOL_GLOBALS
	#define ONPS_MD5_EXT
#else
	#define ONPS_MD5_EXT extern
#endif //* SYMBOL_GLOBALS

 //* MD5摘要值结构体
typedef struct _ST_MD5VAL_ {
	UINT a;
	UINT b;
	UINT c;
	UINT d;
} ST_MD5VAL, *PST_MD5VAL;

ONPS_MD5_EXT ST_MD5VAL onps_md5(UCHAR *pubData, UINT unDataBytes);

#endif

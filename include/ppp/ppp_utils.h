/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.03.21 10:19
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 完成ppp模块相关宏定义、接口函数、结构体定义等工作
 *
 */
#ifndef PPP_UTILS_H
#define PPP_UTILS_H

#ifdef SYMBOL_GLOBALS
	#define PPP_UTILS_EXT
#else
	#define PPP_UTILS_EXT extern
#endif //* SYMBOL_GLOBALS
#include "ppp_frame.h"

PPP_UTILS_EXT const CHAR *get_protocol_name(USHORT usProtocol);
PPP_UTILS_EXT const CHAR *get_cpcode_name(EN_CPCODE enCode); 
PPP_UTILS_EXT const CHAR *get_chap_code_name(EN_CHAPCODE enCode);
PPP_UTILS_EXT const CHAR *get_pap_code_name(EN_PAPCODE enCode);
PPP_UTILS_EXT USHORT ppp_fcs16(UCHAR *pubData, USHORT usDataLen);
PPP_UTILS_EXT USHORT ppp_fcs16_ext(SHORT sBufListHead); 
PPP_UTILS_EXT UINT ppp_escape_encode(UINT unACCM, UCHAR *pubData, UINT unDataLen, UCHAR *pubDstBuf, UINT *punEncodedBytes);
PPP_UTILS_EXT void ppp_escape_encode_init(UINT unACCM, UCHAR ubaACCM[]); 
PPP_UTILS_EXT UINT ppp_escape_encode_ext(UCHAR ubaACCM[], UCHAR *pubData, UINT unDataLen, UCHAR *pubDstBuf, UINT *punEncodedBytes); 
PPP_UTILS_EXT UINT ppp_escape_decode(UCHAR *pubData, UINT unDataLen, UCHAR *pubDstBuf, UINT *punDecodedBytes);
PPP_UTILS_EXT UINT ppp_escape_decode_ext(UCHAR *pubData, UINT unStartIdx, UINT unEndIdx, UINT unDataBufSize, UCHAR *pubDstBuf, UINT *punDecodedBytes);
#endif

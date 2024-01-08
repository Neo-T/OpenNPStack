/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.06.14 16:23
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * ethernet帧结构定义
 *
 */
#ifndef ETHERNET_FRAME_H
#define ETHERNET_FRAME_H
#include "ethernet_protocols.h"

//* ethernet ii协议帧头部结构体
PACKED_BEGIN
typedef struct _ST_ETHERNET_II_HDR_ {
    UCHAR ubaDstMacAddr[ETH_MAC_ADDR_LEN];
    UCHAR ubaSrcMacAddr[ETH_MAC_ADDR_LEN];
    USHORT usProtoType; 
} PACKED ST_ETHERNET_II_HDR, *PST_ETHERNET_II_HDR; 
PACKED_END

PACKED_BEGIN
typedef struct _ST_LOOPBACK_HDR_ {	
	UINT unProtoType;
} PACKED ST_LOOPBACK_HDR, *PST_LOOPBACK_HDR;
PACKED_END

#endif

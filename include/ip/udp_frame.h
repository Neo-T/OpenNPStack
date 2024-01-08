/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.06.02 15:11
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * udp帧结构定义
 *
 */
#ifndef UDP_FRAME_H
#define UDP_FRAME_H

//* UDP帧头部结构体
PACKED_BEGIN
typedef struct _ST_UDP_HDR_ {
    USHORT usSrcPort;
    USHORT usDstPort;    
    USHORT usPacketLen;
    USHORT usChecksum;
} PACKED ST_UDP_HDR, *PST_UDP_HDR;
PACKED_END

#endif

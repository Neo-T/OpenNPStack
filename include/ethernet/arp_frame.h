/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.06.16 13:46
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * arp（Address Resolution Protocol）协议帧结构定义
 *
 */
#ifndef ARP_FRAME_H
#define ARP_FRAME_H

/*
FF FF FF FF FF FF 4e 65 6f 22 06 01 08 06
00 01 08 00 06 04 00 02 02 00 00 00 00 00
c0 a8 00 fc 2c fd a1 ae 27 3e c0 a8 00 03
00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00
*/

#define ARP_HARDWARE_ETH        0x0001  //* 硬件类型为ethernet
#define ARP_HARDWARE_ADDR_LEN   6       //* 硬件地址长度

#define ARP_PROTO_IPv4          0x0800  //* 协议类型为IPv4
#define ARP_PROTO_IPv4_ADDR_LEN 4       //* IPv4协议的地址长度

//* ARP操作码定义
typedef enum {
    ARPOPCODE_REQUEST = 1,
    ARPOPCODE_REPLY = 2,
} EN_ARPOPCODE;

//* arp协议帧头部结构体
PACKED_BEGIN
typedef struct _ST_ARP_HDR_ {
    USHORT usHardwareType;      //* 硬件类型
    USHORT usProtoType;         //* 协议类型
    UCHAR ubHardwareAddrLen;    //* 硬件地址长度
    UCHAR ubProtoAddrLen;       //* 协议地址长度
    USHORT usOptCode;           //* 操作码
} PACKED ST_ARP_HDR, *PST_ARP_HDR; 
PACKED_END

PACKED_BEGIN
typedef struct _ST_ETHIIARP_IPV4_ {
    ST_ARP_HDR stHdr; 
    UCHAR ubaSrcMacAddr[ETH_MAC_ADDR_LEN];
    UINT unSrcIPAddr; 
    UCHAR ubaDstMacAddr[ETH_MAC_ADDR_LEN];
    UINT unDstIPAddr; 
    UCHAR ubaPadding[18];  //* 填充字符，以满足ethernet帧数据域最小长度为46字节的要求
} PACKED ST_ETHIIARP_IPV4, *PST_ETHIIARP_IPV4;
PACKED_END

#endif

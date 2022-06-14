/* ethernet_frame.h
 *
 * ethernet帧结构定义
 *
 * Neo-T, 创建于2022.06.14 16:23
 * 版本: 1.0
 *
 */
#ifndef ETHERNET_FRAME_H
#define ETHERNET_FRAME_H
#include "ethernet_protocols.h"

/*
FF FF FF FF FF FF 4e 65 6f 22 06 01 08 06
00 01 08 00 06 04 00 02 02 00 00 00 00 00
c0 a8 00 fc  2c fd a1 ae 27 3e c0 a8 00 03
00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00
*/

//* PPP帧头部结构体
PACKED_BEGIN
typedef struct _ST_PPP_HDR_ {
	UCHAR ubFlag;		//* 标志域，固定字符（参见PPP_FLAG宏），其界定一个完整的PPP帧
	UCHAR ubAddr;		//* 地址域，固定为PPP_ALLSTATIONS
	UCHAR ubCtl;		//* 控制域，固定为PPP_UI
	USHORT usProtocol;	//* 协议域，PPP帧携带的协议类型，参见ppp_protocols.h中前缀为PPP_的相关宏定义，如PPP_IP为IP协议
} PACKED ST_PPP_HDR, *PST_PPP_HDR;
PACKED_END

#endif

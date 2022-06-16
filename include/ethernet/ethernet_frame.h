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

//* ethernet ii协议帧头部结构体
PACKED_BEGIN
typedef struct _ST_ETHERNET_II_HDR_ {
    UCHAR ubaDstMacAddr[6]; 
    UCHAR ubaSrcMacAddr[6]; 
    USHORT usProtoType; 
} PACKED ST_ETHERNET_II_HDR, *PST_ETHERNET_II_HDR; 
PACKED_END

#endif

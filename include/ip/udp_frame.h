/* udp_frame.h
 *
 * udp帧结构定义
 *
 * Neo-T, 创建于2022.06.02 15:11
 * 版本: 1.0
 *
 */
#ifndef UDP_FRAME_H
#define UDP_FRAME_H

//* 用于校验和计算的UDP伪包头
PACKED_BEGIN
typedef struct _ST_UDP_PSEUDOHDR_ {
    UINT unSrcAddr;
    UINT unDestAddr;
    UCHAR ubMustBeZero;
    UCHAR ubProto;
    USHORT usPacketLen;
} PACKED ST_UDP_PSEUDOHDR, *PST_UDP_PSEUDOHDR;
PACKED_END

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

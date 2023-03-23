/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * udp帧结构定义
 *
 * Neo-T, 创建于2022.06.02 15:11
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

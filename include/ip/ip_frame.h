/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * ip帧结构定义
 *
 * Neo-T, 创建于2022.04.12 10:24
 *
 */
#ifndef IP_FRAME_H
#define IP_FRAME_H

#ifdef SYMBOL_GLOBALS
	#define IP_FRAME_EXT
#else
	#define IP_FRAME_EXT extern
#endif //* SYMBOL_GLOBALS

typedef enum {
    IPPROTO_ICMP = 1,   //* Internet Control Message Protocol
    IPPROTO_IGMP = 2,   //* Internet Gateway Management Protocol
    IPPROTO_TCP  = 6,   //* Transmission Control Protocol    
    IPPROTO_UDP  = 17,  //* User Datagram Protocol        
    IPPROTO_RAW  = 255, //* Raw IP packets
    IPPROTO_MAX
} EN_IPPROTO;

 //* ip帧头部结构体
PACKED_BEGIN
typedef struct _ST_IP_HDR_ {
    UCHAR bitHdrLen     : 4;    //* Length of the header,长度单位为4字节UINT型，不是单字节UCHAR型
    UCHAR bitVer        : 4;    //* version of IP
    UCHAR bitMustBeZero : 1;    //* Must be zero
    UCHAR bitTOS        : 4;    //* Type of service:（占4位从左至右为:最小时延、最大吞吐量、最高可靠性、最小费用，同时只能对1位置位，如4位均位0则表示一般服务）
    UCHAR bitPrior      : 3;    //* Ignore now
    USHORT usPacketLen;         //* Total length of the packet
    USHORT usID;                //* Unique identifier	
    UCHAR bitFragOffset0 : 5;   //* Frag offset0
    UCHAR bitFlag        : 3;   //* Flag	
    UCHAR bitFragOffset1 : 8;   //* Frag offset1
    UCHAR ubTTL;                //* Time to live
    UCHAR ubProto;              //* Protocol(TCP, UDP, etc)
    USHORT usChecksum;          //* IP checksum

    UINT unSrcIP;
    UINT unDstIP;
} PACKED ST_IP_HDR, *PST_IP_HDR;
PACKED_END

#endif

/* ip_frame.h
 *
 * ip帧结构定义
 *
 * Neo-T, 创建于2022.04.12 10:24
 * 版本: 1.0
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
    IPPROTO_IP       = 0,   //* Dummy protocol for TCP
    IPPROTO_HOPOPTS  = 0,   //* IPv6 Hop-by-Hop options
    IPPROTO_ICMP     = 1,   //* Internet Control Message Protocol
    IPPROTO_IGMP     = 2,   //* Internet Gateway Management Protocol
    IPPROTO_IPIP     = 4,   //* IPIP tunnels (older KA9Q tunnels use 94)
    IPPROTO_TCP      = 6,   //* Transmission Control Protocol
    IPPROTO_EGP      = 8,   //* Exterior Gateway Protocol
    IPPROTO_PUP      = 12,  //* PUP protocol
    IPPROTO_UDP      = 17,  //* User Datagram Protocol
    IPPROTO_IDP      = 22,  //* XNS IDP protocol
    IPPROTO_IPV6     = 41,  //* IPv6 header
    IPPROTO_ROUTING  = 43,  //* IPv6 Routing header
    IPPROTO_FRAGMENT = 44,  //* IPv6 fragmentation header
    IPPROTO_ESP      = 50,  //* encapsulating security payload
    IPPROTO_AH       = 51,  //* authentication header
    IPPROTO_ICMPV6   = 58,  //* ICMPv6
    IPPROTO_NONE     = 59,  //* IPv6 no next header
    IPPROTO_DSTOPTS  = 60,  //* IPv6 Destination options
    IPPROTO_RAW      = 255, //* Raw IP packets
    IPPROTO_MAX
} EN_IPPROTO;

 //* ip帧头部结构体
PACKED_BEGIN
typedef struct _ST_IP_HDR_ {
    UCHAR bitLen4Hdr    : 4;    //* Length of the header
    UCHAR bitVer        : 4;    //* version of IP
    UCHAR bitMustBeZero : 1;    //* Must be zero
    UCHAR bitTOS        : 4;    //* Type of service:（占4位从左至右为:最小时延、最大吞吐量、最高可靠性、最小费用，同时只能对1位置位，如4位均位0则表示一般服务）
    UCHAR bitPrior      : 3;    //* Ignore now
    USHORT usPacketLen;         //* Total length of the packet
    USHORT usID;                //* Unique identifier	
    UCHAR bitFragOffset0 : 5;   //* Flag offset0
    UCHAR bitFlag        : 3;   //* Flag	
    UCHAR bitFragOffset1 : 8;   //* Frag offset1
    UCHAR ucTTL;                //* Time to live
    UCHAR ucProto;              //* Protocol(TCP, UDP, etc)
    USHORT usChksum;            //* IP checksum

    UINT unSrcIP;
    UINT unDestIP;
} PACKED ST_IP_HDR, *PST_IP_HDR;
PACKED_END



#endif

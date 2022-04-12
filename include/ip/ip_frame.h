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
typedef struct _ST_PPP_HDR_ {
    UCHAR ubFlag;		//* 标志域，固定字符（参见PPP_FLAG宏），其界定一个完整的PPP帧
    UCHAR ubAddr;		//* 地址域，固定为PPP_ALLSTATIONS
    UCHAR ubCtl;		//* 控制域，固定为PPP_UI
    USHORT usProtocol;	//* 协议域，PPP帧携带的协议类型，参见ppp_protocols.h中前缀为PPP_的相关宏定义，如PPP_IP为IP协议
} PACKED ST_PPP_HDR, *PST_PPP_HDR;
PACKED_END

#endif

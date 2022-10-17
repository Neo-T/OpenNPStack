/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * icmp帧结构定义
 *
 * Neo-T, 创建于2022.04.15 10:09

 *
 */
#ifndef ICMP_FRAME_H
#define ICMP_FRAME_H

typedef enum {
    ICMP_ECHOREPLY      = 0,    //* 回显应答
    ICMP_ERRDST         = 3,    //* 目标网络/主机访问错误
    ICMP_ERRSRC         = 4,    //* 源主机/网络错误
    ICMP_ERRREDIRECT    = 5,    //* 重定向错误
    ICMP_ECHOREQ        = 8,    //* 回显请求 
    ICMP_ROUTEADVERT    = 9,    //* 路由器通告
    ICMP_ROUTESOLIC     = 10,   //* 路由器请求
    ICMP_ERRTTL         = 11,   //* TTL错误
    ICMP_ERRIP          = 12,   //* IP帧错误
    ICMP_MAX            = 255
} EN_ICMPTYPE;

//* ICMP_ERRDST,目标网络/主机访问错误报文携带的具体错误值定义
typedef enum {
    NET_UNREACHABLE      = 0,   //* Network unreachable，网络不可达
    HOST_UNREACHABLE     = 1,   //* Host unreachable，主机不可达
    PROTO_UNREACHABLE    = 2,   //* Protocol unreachable， 协议不可达
    PORT_UNREACHABLE     = 3,   //* Port unreachable， 端口不可达
    NO_FRAGMENT          = 4,   //* Fragmentation needed but no frag. bit set， 需要进行分片但设置不分片比特
    SRCROUTE_FAILED      = 5,   //* Source routing failed，源站选路失败
    DSTNET_UNKNOWN       = 6,   //* Destination network unknown，目的网络未知
    DSTHOST_UNKNOWN      = 7,   //* Destination host unknown，目的主机未知
    SRCHOST_ISOLATED     = 8,   //* Source host isolated (obsolete)，源主机被隔离
    DSTNET_PROHIBITED    = 9,   //* Destination network administratively prohibited，目的网络被强行禁止
    DSTHOST_PROHIBITED   = 10,  //* Destination host administratively prohibited，目的主机被强行禁止
    TOS_NETUNREACHABLE   = 11,  //* Network unreachable for TOS，因TOS问题导致网络不可达
    TOS_HOST_UNREACHABLE = 12,  //* Host unreachable for TOS，因TOS问题导致主机不可达
    COMMU_PROHIBITED     = 13,  //* Communication administratively prohibited by filtering，由于过滤通讯被强行禁止
    HOST_PRECE_VIOLATION = 14,  //* Host precedence violation，主机越权
    PRECE_CUTOFF_EFFECT  = 15   //* Precedence cutoff in effect，优先中止生效
} EN_ERRDST;

//* ICMP_ERRSRC，源主机/网络错误报文携带的具体错误值定义
typedef enum {
    SRC_QUENCH = 0//* Source quench,源端被关闭（基本流控制）
} EN_ERRSRC;

//* ICMP_ERRREDIRECT，重定向错误报文携带的具体错误值定义
typedef enum {
    NET_REDIRECT    = 0,    //* Redirect for network，对网络重定向
    HOST_REDIRECT    = 1,   //* Redirect for host，对主机重定向
    TOSNET_REDIRECT  = 2,   //* Redirect for TOS and network，对服务类型和网络重定向
    TOSHOST_REDIRECT = 3,   //* Redirect for TOS and host，对服务类型和主机重定向
} EN_ERRREDIRECT;

//* ICMP_ERRTTL,TTL错误报文携带的具体错误值定义
typedef enum {
    TRAN_TTL_ZERO     = 0,  //* TTL equals 0 during transit，传输期间生存时间为0
    REASSEMB_TTL_ZERO = 1,  //* TTL equals 0 during reassembly，在数据报组装期间生存时间为0
} EN_ERRTTL;

//* ICMP_ERRIP,IP帧错误报文携带的错误值定义
typedef enum {
    HDR_BAD      = 0,   //* IP header bad (catch all error)，坏的IP首部（包括各种差错）
    OPTIONS_MISS = 1,   //* Required options missing，缺少必需的选项
} EN_ERRIP;

 //* icmp帧头部结构体
PACKED_BEGIN
typedef struct _ST_ICMP_HDR_ {
    UCHAR ubType; 
	UCHAR ubCode; 
	USHORT usChecksum; 	
} PACKED ST_ICMP_HDR, *PST_ICMP_HDR; 
PACKED_END

PACKED_BEGIN
typedef struct _ST_ICMP_ECHO_HDR_ {
    USHORT usIdentifier;
    USHORT usSeqNum;
} PACKED ST_ICMP_ECHO_HDR, *PST_ICMP_ECHO_HDR;
PACKED_END

#endif

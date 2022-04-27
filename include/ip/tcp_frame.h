/* tcp_frame.h
 *
 * tcp帧结构定义
 *
 * Neo-T, 创建于2022.04.25 15:14
 * 版本: 1.0
 *
 */
#ifndef TCP_FRAME_H
#define TCP_FRAME_H

#define TCP_PORT_START 20000 //* 动态分配的起始端口号

//* 用于校验和计算的TCP伪包头
PACKED_BEGIN
typedef struct _ST_TCP_PSEUDOHDR_ {
    UINT unSrcAddr;
    UINT unDestAddr;
    UCHAR ubMustBeZero;
    UCHAR ubProto;
    USHORT usHdrLen;
} PACKED ST_TCP_PSEUDOHDR, *PST_TCP_PSEUDOHDR;
PACKED_END

//* TCP帧头部结构体
PACKED_BEGIN
typedef struct _ST_TCP_HDR_ {
    USHORT usSrcPort;
    USHORT usDestPort;
    UINT unSeqNum;
    UINT unAckNum;

    //* 以下位字段适用于小端
    USHORT 
        bitResrved1 : 3, 
        bitNonce    : 1,
        bitHdrLen   : 4,
        bitFin      : 1,
        bitSyn      : 1,
        bitReset    : 1,
        bitPush     : 1,
        bitAck      : 1,
        bitUrgent   : 1,
        bitResrved2 : 2;

    USHORT usWinSize;
    USHORT usChecksum;
    USHORT usUrgentPointer;
} PACKED ST_TCP_HDR, *PST_TCP_HDR;
PACKED_END

//* TCP选项头部结构
PACKED_BEGIN
typedef struct _ST_TCPOPT_HDR_ {
    UCHAR ubKind;
    UCHAR ubLen;
} PACKED ST_TCPOPT_HDR, *PST_TCPOPT_HDR;
PACKED_END

//* TCP头部选项之MSS
PACKED_BEGIN
typedef struct _ST_TCPOPT_MSS_ {
    ST_TCPOPT_HDR stHdr;
    USHORT usSize;
} PACKED ST_TCPOPT_MSS, *PST_TCPOPT_MSS;
PACKED_END

//* TCP头部选项之时间戳
PACKED_BEGIN
typedef struct _ST_TCPOPT_TIMESTAMP_ {
    ST_TCPOPT_HDR stHdr;
    UINT unTS;
    UINT unTSEcho;
} PACKED ST_TCPOPT_TIMESTAMP, *PST_TCPOPT_TIMESTAMP;
PACKED_END

//* TCP头部选项之窗口扩大因子
PACKED_BEGIN
typedef struct _ST_TCPOPT_WNDSCALE_ {
    ST_TCPOPT_HDR stHdr;
    CHAR bScale; //* 取值范围0~14，即最大扩到65535 * 2^14
} PACKED ST_TCPOPT_WNDSCALE, *PST_TCPOPT_WNDSCALE;
PACKED_END

#endif
/* tcp.h
 *
 * tcp协议相关功能函数
 *
 * Neo-T, 创建于2022.04.25 15:13
 * 版本: 1.0
 *
 */
#ifndef TCP_H
#define TCP_H

#ifdef SYMBOL_GLOBALS
	#define TCP_EXT
#else
	#define TCP_EXT extern
#endif //* SYMBOL_GLOBALS
#include "tcp_frame.h"

typedef enum {
    TLSINIT, //* TCP链路初始状态

    //* 以下为Socket被用于TCP Client时的状态定义
    TLSSYNSENT,             //* 发送SYN请求
    TLSRCVEDSYNACK,         //* 收到SYN ACK
    TLSRCVSYNACKTIMEOUT,    //* 等待接收SYN ACK报文超时
    TLSSYNACKACKSENTFAILED, //* 给服务器发送SYN ACK的ACK报文失败
    TLSCONNECTED,           //* 已连接
    TLSRESET,               //* 连接被重置
    TLSFINSENT,             //* FIN已发送
    TLSRCVEDFINACK,         //* 收到FIN ACK
    TLSCLOSED,              //* 已关闭    

    //* 以下为Socket被用于TCP Server时的状态定义
    TLSSRVSTARTED,  //* TCP Server已启动
    TLSSRVDOWN,     //* TCP Server已关闭
} EN_TCPLINKSTATE;

TCP_EXT INT tcp_send_syn(INT nInput, in_addr_t unSrvAddr, USHORT usSrvPort);


#endif

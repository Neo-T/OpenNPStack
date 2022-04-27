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
    TCSINVALID, //* 状态尚未设置

    //* 以下为Socket被用于TCP Client时的状态定义
    TCSCLIENTINIT = 0,  //* 链路初始状态,
    TCSSYNSENT,         //* 发送SYN请求
    TCSRCVEDSYNACK,     //* 收到SYN ACK
    TCSCONNECTED,       //* 已连接
    TCSRESETTED,        //* 连接被重置
    TCSFINSENT,         //* FIN已发送
    TCSRCVEDFINACK,     //* 收到FIN ACK
    TCSCLOSED,          //* 已关闭

    //* 以下为Socket被用于TCP Server时的状态定义
    TCSSRVINIT,     //* TCP Srv初始状态
    TCSSRVSTARTED,  //* TCP Server已启动
    TCSSRVDOWN,     //* TCP Server已关闭
} EN_TCPCONNSTATE;

#endif

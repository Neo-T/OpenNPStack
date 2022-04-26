/* socket.h
 *
 * 伯克利套接字（Berkeley sockets）实现，实现socket编程需要的基本功能函数，注意：实现并不覆盖全部BSD Socket函数，仅实现常用功能函数
 *
 * Neo-T, 创建于2022.04.26 10:26
 * 版本: 1.0
 *
 */
#ifndef SOCKET_H
#define SOCKET_H

#ifdef SYMBOL_GLOBALS
	#define SOCKET_EXT
#else
	#define SOCKET_EXT extern
#endif //* SYMBOL_GLOBALS
#include "onps_input.h"

typedef INT SOCKET;         //* socket句柄
#define INVALID_SOCKET  -1  //* 无效的SOCKET

//* Supported address families.
#define AF_INET 2   //* internetwork: UDP, TCP, etc.

//* Socket types.
#define SOCK_STREAM 1   //* TCP, stream (connection) socket
#define SOCK_DGRAM  2   //* UDP, datagram (conn.less) socket

//* 参数family仅支持AF_INET，其它不支持，type仅支持SOCK_STREAM、SOCK_DGRAM两种协议，protocol固定为0
SOCKET_EXT int socket(int family, int type, int protocol); 

#endif

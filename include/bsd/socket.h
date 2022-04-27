/* socket.h
 *
 * 伯克利套接字（Berkeley sockets）非标准且不完全实现，按照传统socket编程思想并结合实际应用经验实现的用户层TCP、UDP通讯接口函数，简化了
 * 传统BSD socket需要的一些繁琐的操作，将一些不必要的操作细节改为底层实现，比如select模型、阻塞及非阻塞读写操作等
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

typedef	unsigned int in_addr_t;  //* internet地址类型

//* Supported address families.
#define AF_INET 2   //* internetwork: UDP, TCP, etc.


//* Socket types.
#define SOCK_STREAM 1   //* TCP, stream (connection) socket
#define SOCK_DGRAM  2   //* UDP, datagram (conn.less) socket

//* 参数family仅支持AF_INET，其它不支持，type仅支持SOCK_STREAM、SOCK_DGRAM两种协议，protocol固定为0
SOCKET_EXT SOCKET socket(int family, int type, int protocol); 
SOCKET_EXT void close(SOCKET socket);
SOCKET_EXT int connect(SOCKET socket, const char *srv_ip, unsigned short srv_port);

#endif

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

//* Supported address families.
#define AF_INET 2   //* internetwork: UDP, TCP, etc.

//* Socket types.
#define SOCK_STREAM 1   //* TCP, stream (connection) socket
#define SOCK_DGRAM  2   //* UDP, datagram (conn.less) socket

//* 参数family仅支持AF_INET，其它不支持，type仅支持SOCK_STREAM、SOCK_DGRAM两种协议，protocol固定为0
SOCKET_EXT SOCKET socket(int family, int type, int protocol, EN_ONPSERR *penErr); 
SOCKET_EXT void close(SOCKET socket);

//* 连接函数，阻塞型，直至连接成功或超时，返回0意味着连接成功，-1则意味着连接失败，具体的错误信息通过onps_get_last_error()函数
//* 获得，注意，nConnTimeout参数必须大于0，小于等于0则使用缺省超时时间TCP_CONN_TIMEOUT
SOCKET_EXT int connect(SOCKET socket, const char *srv_ip, unsigned short srv_port, int nConnTimeout);
//* 非阻塞连接函数，连接成功返回0，连接中会一直返回1，返回-1则意味着连接失败，具体的错误信息通过onps_get_last_error()函数获得
SOCKET_EXT int connect_nb(SOCKET socket, const char *srv_ip, unsigned short srv_port); 

//* 发送函数(阻塞型)，直至收到tcp层的ack报文或者超时才会返回，返回值大于0为实际发送的字节数，小于0则发送失败，具体错误信息通过onps_get_last_error()函数获得
SOCKET_EXT int send(SOCKET socket, UCHAR *pubData, INT nDataLen, int nWaitAckTimeout); 
//* 发送函数(非阻塞型)，返回值大于0则为实际发送成功的字节数，等于0为发送中，尚未收到对端的应答，小于0则发送失败，具体错误信息通过onps_get_last_error()函数获得
SOCKET_EXT int send_nb(SOCKET socket, UCHAR *pubData, INT nDataLen);

#endif

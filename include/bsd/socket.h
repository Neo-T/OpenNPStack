/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 伯克利套接字（Berkeley sockets）非标准且不完全实现，按照传统socket编程思想并结合实际应用经验实现的用户层TCP、UDP通讯接口函数，简化了
 * 传统BSD socket需要的一些繁琐的操作，将一些不必要的操作细节改为底层实现，比如select模型、阻塞及非阻塞读写操作等
 * 
 * Neo-T, 创建于2022.04.26 10:26
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

//* tcp链路标志位相关定义
//* =================================================
#define TLF_NO_DELAY_ACK 0x0001 //* 立即回馈应答而不是等一小段时间或有数据需要发送时（200毫秒）再回馈
//* =================================================

//* 参数family仅支持AF_INET，其它不支持，type仅支持SOCK_STREAM、SOCK_DGRAM两种协议，protocol固定为0
SOCKET_EXT SOCKET socket(INT family, INT type, INT protocol, EN_ONPSERR *penErr); 
SOCKET_EXT void close(SOCKET socket);

//* 连接函数，阻塞型，直至连接成功或超时，返回0意味着连接成功，-1则意味着连接失败，具体的错误信息通过onps_get_last_error()函数
//* 获得，注意，nConnTimeout参数必须大于0，小于等于0则函数直接返回，相当于调用connect_nb()函数
SOCKET_EXT INT connect(SOCKET socket, const CHAR *srv_ip, USHORT srv_port, INT nConnTimeout);

//* 功能同上，入口参数与之稍微有些区别，connect()函数的srv_ip参数指向可读的ip地址字符串，connect_ext()函数的srv_ip参数指向的则是
//* inet_addr()/inet6_aton()函数转换后的16进制的实际地址
SOCKET_EXT INT connect_ext(SOCKET socket, void *srv_ip, USHORT srv_port, INT nConnTimeout); 

//* 非阻塞连接函数，连接成功返回0，连接中会一直返回1，返回-1则意味着连接失败，具体的错误信息通过onps_get_last_error()函数获得
SOCKET_EXT INT connect_nb(SOCKET socket, const CHAR *srv_ip, USHORT srv_port); 

//* 功能同上，入口参数与connect_nb()函数的区别同同connect_ext()函数
SOCKET_EXT INT connect_nb_ext(SOCKET socket, void *srv_ip, USHORT srv_port);


//* 发送函数(tcp链路下阻塞型)，直至收到tcp层的ack报文或者超时才会返回，返回值大于0为实际发送的字节数，小于0则发送失败，具体错误信息通过onps_get_last_error()函数获得
//* 对于udp协议来说参数nWaitAckTimeout将被忽略，其将作为非阻塞型函数使用；
//* 当使能tcp的SUPPORT_SACK选项支持时，tcp链路下参数nWaitAckTimeout无效，可以是任意值，此时该函数为非阻塞型，其仅仅是把数据写入tcp链路的发送缓冲区，返回值为实际写入
//* 的数据长度，返回值为0代表缓冲区已满，小于0则发生错误，具体的错误信息通过onps_get_last_error()函数获得
SOCKET_EXT INT send(SOCKET socket, UCHAR *pubData, INT nDataLen, INT nWaitAckTimeout); 

//* 发送函数(tcp链路下非阻塞型)，udp链路该函数与send()函数功能及实现逻辑完全相同
SOCKET_EXT INT send_nb(SOCKET socket, UCHAR *pubData, INT nDataLen);

//* 检查当前tcp链路发送的数据是否已成功送达对端，返回值为1则成功送达对端（收到tcp ack报文）；等于0为发送中，尚未收到对端的应答；小于0则发送失败，具体错误信息通过onps_get_last_error()函数获得
SOCKET_EXT INT is_tcp_send_ok(SOCKET socket);

//* 仅用于udp发送，发送时指定目标地址
SOCKET_EXT INT sendto(SOCKET socket, const CHAR *dest_ip, USHORT dest_port, UCHAR *pubData, INT nDataLen);

//* 设定recv()函数等待接收的时长（单位：秒），大于0指定数据到达的最长等待时间；0，则不等待；-1，则一直等待直至数据到达或报错
SOCKET_EXT BOOL socket_set_rcv_timeout(SOCKET socket, CHAR bRcvTimeout, EN_ONPSERR *penErr);

//* 修改tcp链路相关控制标志
SOCKET_EXT BOOL socket_set_tcp_link_flags(SOCKET socket, USHORT usNewFlags, EN_ONPSERR *penErr); 

//* 修改tcp链路相关控制标志
SOCKET_EXT BOOL socket_set_tcp_link_flags_safe(SOCKET socket, USHORT usNewFlags, EN_OPTTYPE enOptType, EN_ONPSERR *penErr);

//* 接收函数(阻塞型/非阻塞型)，依赖于socket_set_rcv_timeout()函数设定的接收等待时长，缺省为一直等待直至收到数据或报错，阻塞型返回值为实际收到的数据长度，-1则代
//* 表出错；非阻塞型返回值为实际收到的数据长度（大于等于0），-1同样代表接收失败
SOCKET_EXT INT recv(SOCKET socket, UCHAR *pubDataBuf, INT nDataBufSize); 

//* 接收函数，仅用于udp协议接收
SOCKET_EXT INT recvfrom(SOCKET socket, UCHAR *pubDataBuf, INT nDataBufSize, void *pvFromIP, USHORT *pusFromPort);

//* 获取当前tcp连接状态，0：未连接；1：已连接；-1：读取状态失败，具体错误信息由参数penErr返回
SOCKET_EXT INT is_tcp_connected(SOCKET socket, EN_ONPSERR *penErr); 

//* 为socket绑定指定的网络地址和端口，如果想绑定任意网络接口地址，参数pszNetifIp为NULL即可
SOCKET_EXT INT bind(SOCKET socket, const CHAR *pszNetifIp, USHORT usPort);

#if SUPPORT_ETHERNET
//* tcp服务器进入被动监听状态，入口参数与函数功能与伯克利sockets完全相同
SOCKET_EXT INT listen(SOCKET socket, USHORT backlog); 

//* 接受一个到达的tcp连接请求，参数punFromIP及pusFromPort为发起连接请求的客户端的ip地址及端口，参数nWaitSecs指定等待时长（单位秒）：
//*     0: 不等待，立即返回
//* 大于0: 等待指定时间直至超时或收到一个到达的连接请求
//* 小于0: 一直等待，直至收到一个到达的连接请求
SOCKET_EXT SOCKET accept(SOCKET socket, void *pvCltIP, USHORT *pusCltPort, INT nWaitSecs, EN_ONPSERR *penErr);

//* 轮询等待tcp服务器数据到达，入口参数hSocketSrv为tcp服务器的socket，不是客户端的socket，nWaitSecs指定等待的秒数：
//*     0: 不等待，立即返回
//* 大于0: 等待指定时间直至超时或某个客户端链路收到数据
//* 小于0: 一直等待，直至某个或多个客户端链路收到数据
//* 返回值为收到数据的客户端socket
SOCKET_EXT SOCKET tcpsrv_recv_poll(SOCKET hSocketSrv, INT nWaitSecs, EN_ONPSERR *penErr);

//* 设置服务器的接收模式
//* TCPSRVRCVMODE_ACTIVE 主动读取方式，需要用户通过recv()函数主动遍历读取每个客户端到达的数据
//* TCPSRVRCVMODE_POLL
SOCKET_EXT BOOL tcpsrv_set_recv_mode(SOCKET hSocketSrv, CHAR bRcvMode, EN_ONPSERR *penErr);

//* 启动tcp服务器
SOCKET_EXT SOCKET tcpsrv_start(INT family, USHORT usSrvPort, USHORT usBacklog, CHAR bRcvMode, EN_ONPSERR *penErr);
#endif

//* 连接tcp服务器
SOCKET_EXT SOCKET tcp_srv_connect(INT family, void *srv_ip, USHORT srv_port, INT nRcvTimeout, INT nConnTimeout, EN_ONPSERR *penErr);

//* tcp数据发送函数，相对于传统send()函数该函数增加了容错处理逻辑
SOCKET_EXT BOOL tcp_send(SOCKET hSocket, UCHAR *pubData, INT nDataLen); 

//* 获取最近一次发生的错误信息
SOCKET_EXT const CHAR *socket_get_last_error(SOCKET socket, EN_ONPSERR *penErr);

//* 获取最近一次发生的错误码
SOCKET_EXT EN_ONPSERR socket_get_last_error_code(SOCKET socket); 

#endif

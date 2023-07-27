/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * icmp/tcp/udp层接收处理逻辑用到的基础数据结构、辅助宏、相关功能函数等的定义与声明工作
 *
 * Neo-T, 创建于2022.04.13 17:25
 *
 */
#ifndef ONPSINPUT_H
#define ONPSINPUT_H

#ifdef SYMBOL_GLOBALS
	#define ONPSINPUT_EXT
#else
	#define ONPSINPUT_EXT extern
#endif //* SYMBOL_GLOBALS
#include "ip/ip.h"

#define ONPS_OFFICIAL_WEB   "http://www.onps.org.cn/"
#define ONPS_VER            "1.1.0.230726"

#define INVALID_INPUT -1 //* 无效的输入控制块

 //* Supported address families.
#define AF_INET  2  //* internetwork: UDP, TCP, etc.
#if SUPPORT_IPV6
#define AF_INET6 23 // Internetwork Version 6
#endif

 //* Socket types.
#define SOCK_STREAM 1   //* TCP, stream (connection) socket
#define SOCK_DGRAM  2   //* UDP, datagram (conn.less) socket

//* 协议栈支持的输入控制块相关配置项定义
typedef enum {    
    IOPT_SETICMPECHOID = 0,  //* 设置icmp echo请求ID
    IOPT_SETTCPUDPADDR,      //* 设置TCP/UDP本地分配的地址
    IOPT_SETTCPLINKFLAGS,    //* 设置tcp链路标志
    IOPT_FREETCPSRVRCVBUF,   //* 释放tcp服务器的接收缓冲区，因为服务器socket不需要接收缓冲区，在调用bind()函数时应该释放（建立socket时自动分配了接收缓冲区）
    IOPT_GETTCPUDPADDR,      //* 获取TCP/UDP本地分配的地址
    IOPT_GETSEM,             //* 获取input用到的semaphore
    IOPT_GETIPPROTO,         //* 获取当前input绑定的ip上层协议
    IOPT_GETTCPLINKSTATE,    //* 获取tcp链路状态
    IOPT_SETTCPLINKSTATE,    //* 设置tcp链路状态
    IOPT_SETATTACH,          //* 设置附加信息
    IOPT_GETATTACH,          //* 获取附加信息地址
    IOPT_GETTCPUDPLINK,      //* 获取tcp/udp链路状态
    IOPT_GETTCPDATASNDSTATE, //* 获取tcp链路数据发送的状态
    IOPT_SETRCVTIMEOUT,      //* 设置接收等待时长（单位：秒）
    IOPT_GETRCVTIMEOUT,      //* 获取接收等待时长
    IOPT_GETLASTSNDBYTES,    //* 获取最近一次数据发送长度        
    IOPT_GETTCPLINKFLAGS,    //* 读取tcp链路标志
#if SUPPORT_IPV6
	IOPT_GETICMPAF,          //* 读取icmp协议的地址族类型
#endif    
} ONPSIOPT;

#if SUPPORT_IPV6
PACKED_BEGIN
typedef struct _STP_SOCKADDR_ {	
	USHORT usPort; 
	union
	{
		UINT unVal;
		UCHAR ubaVal[16];
	} PACKED uniIp;
} PACKED STP_SOCKADDR, *PSTP_SOCKADDR;
PACKED_END

typedef struct _ST_SOCKADDR_ {
	USHORT usPort;
	union
	{
		UINT unVal;
		UCHAR ubaVal[16];
	} uniIp;
} ST_SOCKADDR, *PST_SOCKADDR; 
#else
typedef struct _ST_SOCKADDR_ {
    USHORT usPort; 
    UINT unIp; 
} ST_SOCKADDR, *PST_SOCKADDR;
#endif

#define TCP_TYPE_LCLIENT 0  //* 连接远端服务器的本地tcp客户端
#define TCP_TYPE_RCLIENT 1  //* 连接本地服务器的远端tcp客户端
#define TCP_TYPE_SERVER  2  //* 本地tcp服务器
typedef struct _ST_TCPUDP_HANDLE_ {
    CHAR bType;    //* 仅用于tcp链路，udp链路忽略该字段，用于标识这是否是服务器、连接本地服务器的客户端、连接远端服务器的客户端（udp客户端与服务器的处理逻辑本质上完全相同，不需要单独区分）    
#if SUPPORT_IPV6
	CHAR bFamily;  //* 协议族标识，这里用于区分底层协议族为ipv4还是ipv6
	ST_SOCKADDR stSockAddr; 		
#else
	struct {
		USHORT usPort;
		UINT unIp;
	} stSockAddr; 
#endif
} ST_TCPUDP_HANDLE, *PST_TCPUDP_HANDLE;
#if SUPPORT_IPV6
#define saddr_ipv4 uniIp.unVal
#define saddr_ipv6 uniIp.ubaVal
#else
#define saddr_ipv4 unIp
#endif

typedef struct _ST_TCPLINK_ ST_TCPLINK, *PST_TCPLINK; 

//* 输入控制块初始化
ONPSINPUT_EXT BOOL onps_input_init(EN_ONPSERR *penErr); 

//* 去初始化输入控制块
ONPSINPUT_EXT void onps_input_uninit(void); 

//* 建立一个新的输入控制块
#if !SUPPORT_IPV6
ONPSINPUT_EXT INT onps_input_new(EN_IPPROTO enProtocol, EN_ONPSERR *penErr); 
#else
ONPSINPUT_EXT INT onps_input_new(INT family, EN_IPPROTO enProtocol, EN_ONPSERR *penErr);
#endif
#if SUPPORT_ETHERNET
ONPSINPUT_EXT INT onps_input_new_tcp_remote_client(INT nInputSrv, USHORT usSrvPort, void *pvSrvIp, USHORT usCltPort, void *pvCltIp, PST_TCPLINK *ppstTcpLink, EN_ONPSERR *penErr);
#endif

//* 释放一个输入控制块
ONPSINPUT_EXT void onps_input_free(INT nInput); 

//* 设置/获取相关配置项
ONPSINPUT_EXT BOOL onps_input_set(INT nInput, ONPSIOPT enInputOpt, void *pvVal, EN_ONPSERR *penErr);
ONPSINPUT_EXT BOOL onps_input_get(INT nInput, ONPSIOPT enInputOpt, void *pvVal, EN_ONPSERR *penErr);

//* 投递一个数据到达或链路异常的信号
ONPSINPUT_EXT void onps_input_sem_post(INT nInput); 
ONPSINPUT_EXT INT onps_input_sem_pend(INT nInput, INT nWaitSecs, EN_ONPSERR *penErr);
ONPSINPUT_EXT INT onps_input_sem_pend_uncond(INT nInput, INT nWaitSecs, EN_ONPSERR *penErr);

#if SUPPORT_ETHERNET
ONPSINPUT_EXT void onps_input_sem_post_tcpsrv_accept(INT nSrvInput, INT nCltInput, UINT unLocalSeqNum); 
#endif

//* 对tcp链路关闭状态进行迁移
ONPSINPUT_EXT BOOL onps_input_set_tcp_close_state(INT nInput, CHAR bDstState); 
//* tcp链路关闭操作定时器计数函数
ONPSINPUT_EXT INT onps_input_tcp_close_time_count(INT nInput);

//* input层未tcp之类的ip上层数据流协议提供的线程锁，确保发送序号不出现乱序的情形
ONPSINPUT_EXT void onps_input_lock(INT nInput); 
ONPSINPUT_EXT void onps_input_unlock(INT nInput);

//* 根据对端发送的标识获取本地icmp句柄
ONPSINPUT_EXT INT onps_input_get_icmp(USHORT usIdentifier);

//* 将底层协议收到的对端发送过来的数据放入接收缓冲区，onps_input_recv()函数用于icmp及udp协议的接收，onps_input_tcp_recv()用户tcp协议的接收
#if SUPPORT_IPV6
ONPSINPUT_EXT BOOL onps_input_recv(INT nInput, const UCHAR *pubData, INT nDataBytes, void *pvFromIP, USHORT usFromPort, EN_ONPSERR *penErr);
#else
ONPSINPUT_EXT BOOL onps_input_recv(INT nInput, const UCHAR *pubData, INT nDataBytes, in_addr_t unFromIP, USHORT usFromPort, EN_ONPSERR *penErr);
#endif

#define TCPSRVRCVMODE_ACTIVE 0 //* 采用主动读取方式，需要用户通过recv()函数主动遍历读取每个客户端到达的数据
#define TCPSRVRCVMODE_POLL   1 //* 采用poll模型读取数据，用户通过tcpsrv_recv_poll()函数等待数据到达信号
ONPSINPUT_EXT INT onps_input_tcp_recv(INT nInput, const UCHAR *pubData, INT nDataBytes, EN_ONPSERR *penErr);

//* 将收到的数据推送给用户层
ONPSINPUT_EXT INT onps_input_recv_upper(INT nInput, UCHAR *pubDataBuf, UINT unDataBufSize, void *pvFromIP, USHORT *pusFromPort, EN_ONPSERR *penErr);

//* 等待接收icmp层对端发送的数据
ONPSINPUT_EXT INT onps_input_recv_icmp(INT nInput, UCHAR **ppubPacket, void *pvSrcAddr, UCHAR *pubTTL, UCHAR *pubType, UCHAR *pubCode, INT nWaitSecs, EN_ONPSERR *penErr);

//* 检查要某个端口是否已被使用   
#if SUPPORT_IPV6
ONPSINPUT_EXT BOOL onps_input_port_used(INT nFamily, EN_IPPROTO enProtocol, USHORT usPort);
#else
ONPSINPUT_EXT BOOL onps_input_port_used(EN_IPPROTO enProtocol, USHORT usPort);
#endif

//* 分配一个动态端口
#if SUPPORT_IPV6
ONPSINPUT_EXT USHORT onps_input_port_new(INT nFamily, EN_IPPROTO enProtocol);
#else
ONPSINPUT_EXT USHORT onps_input_port_new(EN_IPPROTO enProtocol);
#endif

//* 根据ip地址和端口号获取input句柄
#if SUPPORT_ETHERNET
ONPSINPUT_EXT INT onps_input_get_handle_of_tcp_rclient(void *pvSrvIp, USHORT usSrvPort, void *pvCltIp, USHORT usCltPort, PST_TCPLINK *ppstTcpLink); 
#endif
#if SUPPORT_IPV6
ONPSINPUT_EXT INT onps_input_get_handle(INT family, EN_IPPROTO enIpProto, void *pvNetifIp, USHORT usPort, void *pvAttach);
#else
ONPSINPUT_EXT INT onps_input_get_handle(EN_IPPROTO enIpProto, UINT unNetifIp, USHORT usPort, void *pvAttach);
#endif

//* 设置/获取最近一次发生的错误
ONPSINPUT_EXT const CHAR *onps_get_last_error(INT nInput, EN_ONPSERR *penErr);
ONPSINPUT_EXT EN_ONPSERR onps_get_last_error_code(INT nInput);
ONPSINPUT_EXT void onps_set_last_error(INT nInput, EN_ONPSERR enErr);

#if SUPPORT_SACK
ONPSINPUT_EXT INT onps_tcp_send(INT nInput, UCHAR *pubData, INT nDataLen);
#endif

#endif

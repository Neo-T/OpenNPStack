/* onps_input.h
 *
 * icmp/tcp/udp层接收处理逻辑用到的基础数据结构、辅助宏、相关功能函数等的定义与声明工作
 *
 * Neo-T, 创建于2022.04.13 17:25
 * 版本: 1.0
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

//* 协议栈支持的输入控制块相关配置项定义
typedef enum {
    IOPT_RCVBUFSIZE = 0,        //* 设置接收缓冲区大小
    IOPT_SETICMPECHOID,         //* 设置icmp echo请求ID
    IOPT_SETTCPUDPADDR,         //* 设置TCP/UDP本地分配的地址
    IOPT_GETTCPUDPADDR,         //* 获取TCP/UDP本地分配的地址
    IOPT_GETSEM,                //* 获取input用到的semaphore
    IOPT_GETIPPROTO,            //* 获取当前input绑定的ip上层协议
    IOPT_GETTCPLINKSTATE,       //* 获取tcp链路状态
    IOPT_SETTCPLINKSTATE,       //* 设置tcp链路状态
    IOPT_SETATTACH,             //* 设置附加信息
    IOPT_GETATTACH,             //* 获取附加信息地址
    IOPT_GETTCPDATASNDSTATE,    //* 获取tcp链路数据发送的状态
    IOPT_SETRCVTIMEOUT,         //* 设置接收等待时长（单位：秒）
    IOPT_GETRCVTIMEOUT,         //* 获取接收等待时长
    IOPT_GETLASTSNDBYTES,       //* 获取最近一次数据发送长度
} ONPSIOPT;

typedef struct _ST_TCPUDP_HANDLE_ {
    UINT unNetifIp;
    USHORT usPort;    
} ST_TCPUDP_HANDLE, *PST_TCPUDP_HANDLE;

//* 输入控制块初始化
ONPSINPUT_EXT BOOL onps_input_init(EN_ONPSERR *penErr); 

//* 去初始化输入控制块
ONPSINPUT_EXT void onps_input_uninit(void); 

//* 建立一个新的输入控制块
ONPSINPUT_EXT INT onps_input_new(EN_IPPROTO enProtocol, EN_ONPSERR *penErr); 

//* 释放一个输入控制块
ONPSINPUT_EXT void onps_input_free(INT nInput); 

//* 设置/获取相关配置项
ONPSINPUT_EXT BOOL onps_input_set(INT nInput, ONPSIOPT enInputOpt, void *pvVal, EN_ONPSERR *penErr);
ONPSINPUT_EXT BOOL onps_input_get(INT nInput, ONPSIOPT enInputOpt, void *pvVal, EN_ONPSERR *penErr);

//* 根据对端发送的标识获取本地icmp句柄
ONPSINPUT_EXT INT onps_input_get_icmp(USHORT usIdentifier);

//* 将底层协议收到的对端发送过来的数据放入接收缓冲区
ONPSINPUT_EXT BOOL onps_input_recv(INT nInput, const UCHAR *pubData, INT nDataByte, EN_ONPSERR *penErrs);

//* 将收到的数据推送给用户层
ONPSINPUT_EXT INT onps_input_recv_upper(INT nInput, UCHAR *pubDataBuf, UINT unDataBufSize, EN_ONPSERR *penErr);

//* 等待接收icmp层对端发送的数据
ONPSINPUT_EXT INT onps_input_recv_icmp(INT nInput, UCHAR **ppubPacket, UINT *punSrcAddr, UCHAR *pubTTL, INT nWaitSecs); 

//* 分配一个动态端口
ONPSINPUT_EXT USHORT onps_input_port_new(EN_IPPROTO enProtocol);

//* 根据ip地址和端口号获取input句柄
ONPSINPUT_EXT INT onps_input_get_handle(UINT unNetifIp, USHORT usPort); 
ONPSINPUT_EXT INT onps_input_get_handle_ext(UINT unNetifIp, USHORT usPort, void *pvAttach);

//* 设置/获取最近一次发生的错误
ONPSINPUT_EXT const CHAR *onps_get_last_error(INT nInput, EN_ONPSERR *penErr);
ONPSINPUT_EXT void onps_set_last_error(INT nInput, EN_ONPSERR enErr);

#endif

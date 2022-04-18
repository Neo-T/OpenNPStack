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
    IOPT_RCVBUFSIZE = 0,    //* 设置接收缓冲区大小
    IOPT_SETICMPECHOID,     //* 设置icmp echo请求ID
    IOPT_SETIP,             //* 地址IP地址
    IOPT_SETPORT,           //* 设置端口
} ONPSIOPT;

ONPSINPUT_EXT BOOL onps_input_init(EN_ONPSERR *penErr);  //* 输入控制块初始化
ONPSINPUT_EXT void onps_input_uninit(void); //* 去初始化输入控制块
ONPSINPUT_EXT INT onps_input_new(EN_IPPROTO enProtocol, EN_ONPSERR *penErr);  //* 建立一个新的输入控制块
ONPSINPUT_EXT void onps_input_free(INT nInput);  //* 释放一个输入控制块
ONPSINPUT_EXT BOOL onps_input_set(INT nInput, ONPSIOPT enInputOpt, void *pvVal, EN_ONPSERR *penErr);
ONPSINPUT_EXT INT onps_input_get_icmp(USHORT usIdentifier);
ONPSINPUT_EXT UCHAR *onps_input_get_rcv_buf(INT nInput, HSEM *phSem, UINT *punRcvedBytes);
ONPSINPUT_EXT INT onps_input_recv_icmp(INT nInput, UCHAR **ppubPacket, INT nWaitSecs); 

#endif

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

//* 协议栈icmp/tcp/udp层接收处理节点
typedef struct _STCB_ONPS_INPUT_ {
    HSEM hSem;          //* 信号量，一旦收到数据，协议栈将投递信号量给上层接收者，避免上层调用者采用轮询读取数据的方式，避免CPU资源被过多占用
    UCHAR ubIPProto;    //* IP上层协议定义，目前支持icmp/tcp/udp接收，其值来自于ip_frame.h中EN_IPPROTO枚举定义
   
    union {  //* 系统分配的接收者句柄，根据不同的上层协议其句柄信息均有所不同
        struct {
            USHORT usIdentifier;
        } stIcmp; //* icmp层句柄

        struct {            
            USHORT usPort; 
            UINT unIP;
        } stTcp; //* tcp层句柄，使用IP地址和端口就可以唯一的标识一个tcp连接

        struct {            
            USHORT usPort;
            UINT unIP;
        } stUdp; //* udp层句柄，同tcp
    } uniHandle;
    UCHAR *pubRcvBuf; 
    UINT unRcvBufSize; 
} STCB_ONPS_INPUT, *PSTCB_ONPS_INPUT;

ONPSINPUT_EXT BOOL onps_input_init(EN_ERROR_CODE *penErrCode);  //* 输入控制块初始化
ONPSINPUT_EXT void onps_input_uninit(void); //* 去初始化输入控制块
ONPSINPUT_EXT PSTCB_ONPS_INPUT onps_input_new(EN_IPPROTO enProtocol, EN_ERROR_CODE *penErrCode);  //* 建立一个新的输入控制块
ONPSINPUT_EXT void onps_input_free(PSTCB_ONPS_INPUT pstInput);  //* 释放一个输入控制块

#endif

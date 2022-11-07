/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 与目标操作系统相关的通用数据类型定义
 *
 * Neo-T, 创建于2022.03.14 15:56
 *
 */
#ifndef OS_DATATYPE_H
#define OS_DATATYPE_H

typedef INT HMUTEX;			//* 线程同步锁句柄
#define INVALID_HMUTEX -1	//* 无效的线程同步锁句柄

#if SUPPORT_PPP
typedef INT HTTY;			//* tty终端句柄
#define INVALID_HTTY -1		//* 无效的tty终端句柄
#endif

typedef INT HSEM;			//* 信号量，适用与不同线程间通讯
#define INVALID_HSEM -1		//* 无效的信号量句柄

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef unsigned int BOOL;  //* bool型变量
#ifndef TRUE
#define TRUE 	1
#endif

#ifndef FALSE
#define FALSE 	0
#endif

#ifndef s_addr  //* Internet address
struct in_addr
{
    in_addr_t s_addr;
};
#define s_addr s_addr
#endif

#endif


/* onps_errors.h
 *
 * 错误类型定义
 *
 * Neo-T, 创建于2022.03.14 17:14
 * 版本: 1.0
 *
 */
#ifndef ONPS_ERRORS_H
#define ONPS_ERRORS_H

#ifdef SYMBOL_GLOBALS
#define ONPS_ERRORS_EXT
#else
#define ONPS_ERRORS_EXT extern
#endif //* SYMBOL_GLOBALS

typedef enum {
    ERRNO = 0,          //* 没有发生任何错误
    ERRNOPAGENODE,      //* 无可用的内存页面节点
    ERRREQMEMTOOLARGE,  //* 申请的内存过大，超过了系统支持的最大申请配额
    ERRNOFREEMEM,       //* 系统已无可用内存
    ERRMUTEXINITFAILED, //* 线程同步锁初始化失败
    ERRNOBUFLISTNODE,   //* 无缓冲区链表节点
    ERRSEMINITFAILED,   //* 信号量初始化失败
    ERROPENTTY,         //* tty终端打开失败
    ERRATWRITE,         //* AT指令写错误
    ERRATEXEC,          //* AT指令返回错误
    ERRATEXECTIMEOUT,   //* AT指令执行超时
    ERRSIMCARD,         //* 未检测到SIM卡
    ERRREGMOBILENET,    //* 没有注册到移动网络
    ERRPPPDELIMITER,    //* 未找到ppp帧定界符
    ERRPPPFCS,          //* ppp帧校验和错误
    ERRTOOMANYTTYS,     //* tty数量过多
    ERRTTYHANDLE,       //* tty句柄无效
    ERROSADAPTER,       //* os适配层错误
    ERRUNKNOWNPROTOCOL, //* 未知协议类型
    ERRNOIDLETIMER,     //* 没有空闲的定时器
    ERRPPPWALISTNOINIT, //* ppp等待应答链表未初始化
    ERRNONETIFNODE,     //* 无可用的netif节点
    ERRUNSUPPIPPROTO,   //* 不被支持的IP层协议
    ERRUNSUPPIOPT,      //* 不支持的配置项
    ERRIPROTOMATCH,     //* 协议匹配错误
    ERRNOROUTENODE,     //* 无可用的路由表单元
} EN_ONPSERR;

typedef struct _ST_ONPSERR_ {
    EN_ONPSERR enCode;
    CHAR szDesc[128];
} ST_ONPSERR, *PST_ONPSERR;

ONPS_ERRORS_EXT const CHAR *onps_error(EN_ONPSERR enErr);

#endif

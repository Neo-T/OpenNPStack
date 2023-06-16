/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 实现telnet通讯协议，提供telnet基础通讯用的相关功能函数
 * 
 * Neo-T, 创建于2023.05.26 10:48
 *
 */
#ifndef TELNET_H
#define TELNET_H

#ifdef SYMBOL_GLOBALS
	#define TELNET_EXT
#else
	#define TELNET_EXT extern
#endif //* SYMBOL_GLOBALS

#if NETTOOLS_TELNETCLT || NETTOOLS_TELNETSRV

 //* 以下为Telnet命令（Telnet Commands）定义
 //* 这之下的命令由[RFC1184]定义，详见：https://www.rfc-editor.org/rfc/rfc1184
#define TELNETCMD_EOF   236 //* End of File，文件结束符
#define TELNETCMD_SUSP  237 //* Suspend the execution of the current process，挂起当前正在执行的进程
#define TELNETCMD_ABORT 238 //* Abort the process，异常中止进程

 //* 由[RFC885]第6节“Implementation Considerations”定义，详见：https://www.rfc-editor.org/rfc/rfc885
#define TELNETCMD_EOR   239 //* End Of Record，记录结束符

 //* 这之下的命令由[RFC854]定义，详见：https://www.rfc-editor.org/rfc/rfc854#page-14
 //* telnet在进入正常工作状态之前首先要进行选项协商。任何一方都可以主动发送选项协商请求给对方。协商请求包括以下4种命令字：
 //* WILL（TELNETCMD_WILL）、WONT（TELNETCMD_WONT）、DO（TELNETCMD_DO）、DONT（TELNETCMD_DONT）
 //* 对于激活选项请求，对端有权同意或者不同意；而对于禁止选项请求，则必须同意。如此，以上4种请求存在6种情况：
 //* 发送方        接收方        含义
 //* ------------------------------------------------
 //*  WILL   ->             发送方想激活选项
 //*         <-      DO      接受方同意激活
 //* ------------------------------------------------
 //*  WILL  ->              发送方想激活选项
 //*        <-      DONT    接收方不同意激活
 //* ------------------------------------------------
 //*   DO   ->              发送方想让接收方激活选项
 //*        <-      WILL    接收方同意激活
 //* ------------------------------------------------
 //*   DO   ->              发送方想让接收方激活选项
 //*        <-      WONT    接收方不同意激活
 //* ------------------------------------------------
 //*  WONT  ->              发送方想要禁止选项
 //*        <-      DONT    接收方必须同意对端提出的禁止选项的请求
 //* ------------------------------------------------
 //*  DONT  ->              发送方想让接收方禁止选项
 //*        <-      WONT    接收方必须同意对端提出的本地禁止选项的请求
 //* ------------------------------------------------
#define TELNETCMD_SE    240 //* End of subnegotiation parameters，子选项结束
#define TELNETCMD_NOP   241 //* No operation，无操作
#define TELNETCMD_DM    242 //* Data Mark，数据标记
#define TELNETCMD_BRK   243 //* NVT character BRK，NVT的BRK字符
#define TELNETCMD_IP    244 //* Interrupt Process，中断进程
#define TELNETCMD_AO    245 //* Abort output，异常中止输出
#define TELNETCMD_AYT   246 //* Are You There，对方是否还在运行
#define TELNETCMD_EC    247 //* Erase character，转义字符
#define TELNETCMD_EL    248 //* Erase line，删除行
#define TELNETCMD_GA    249 //* Go ahead，继续进行信号，即GA信号
#define TELNETCMD_SB    250 //* Subnegotiation，子选项开始标志符
#define TELNETCMD_WILL  251 //* 选项协商，发送方本地将激活选项
#define TELNETCMD_WONT  252 //* 选项协商，发送方本地想禁止选项
#define TELNETCMD_DO    253 //* 选项协商，发送方想叫接收方激活选项
#define TELNETCMD_DONT  254 //* 选项协商，发送方想让接收方去禁止选项
#define TELNETCMD_IAC   255 //* Interpret As Command，后面紧跟的一个字节要作为命令字来解释

 //* Telnet选项定义，详见：https://www.iana.org/assignments/telnet-options/telnet-options.xhtml#telnet-options-1
 //* 这里只定义协议栈支持的相关选项
#define TELNETOPT_ECHO      1   //* Echo，回显
#define TELNETOPT_SGA       3   //* Suppress Go Ahead，抑制GA信号，NVT默认为半双工设备，在接收用户输入之前必须从服务器进程获得GA信号（其实就是TELNETCMD_GA命令），通过SGA选项禁止发送GA信号则可以让NVT无缝切换用户输入与输出而不再等待GA信号
#define TELNETOPT_TM        6   //* Timing Mark，用于收发双方同步的选项，在这里其被用于“准行方式”协商，其完整使用说明详见[RFC860]：https://www.rfc-editor.org/rfc/rfc860
#define TELNETOPT_TERMTYPE  24  //* Terminal Type，终端类型，详见[RFC1091]：https://www.rfc-editor.org/rfc/rfc1091.html

#endif
#endif

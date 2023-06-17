/* telnet_srv.h
*
* telnet服务器相关的一些宏以及基础数据结构定义
*
* Neo-T, 创建于2022.05.30 11:56
*
*/
#ifndef TELNETSRV_H
#define TELNETSRV_H

#ifdef SYMBOL_GLOBALS
#define TELNETSRV_EXT
#else
#define TELNETSRV_EXT extern
#endif //* SYMBOL_GLOBALS

#if NETTOOLS_TELNETSRV
#include "net_virtual_terminal.h"

#define TELNETSRV_PORT 23 //* telnet服务器端口
#define TELNETCLT_NUM  6  //* 指定系统允许的telnet客户端数量

#define TELNETCLT_INACTIVE_TIMEOUT  300 //* 定义telnet客户端最长多少秒可以无任何操作，超过这个时间服务器会主动断开当前连接

TELNETSRV_EXT BOOL nvt_start(PSTCB_TELNETCLT pstcbTelnetClt);
TELNETSRV_EXT void nvt_stop(PSTCB_TELNETCLT pstcbTelnetClt);
TELNETSRV_EXT void telnet_srv_entry(void *pvParam);
TELNETSRV_EXT BOOL telnet_srv_end(void);
#endif
#endif

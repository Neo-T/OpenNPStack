/* telnet_client.h
*
* telnet客户端相关基础数据结构及宏定义文件
*
* Neo-T, 创建于2022.06.21 11:22
*
*/
#ifndef TELNET_CLIENT_H
#define TELNET_CLIENT_H

#ifdef SYMBOL_GLOBALS
#define TELNET_CLIENT_EXT
#else
#define TELNET_CLIENT_EXT extern
#endif //* SYMBOL_GLOBALS

//* telnet客户端启动参数
typedef struct _ST_TELCLT_STARTARGS_ {
    CHAR bIsCpyEnd; 
    ULONGLONG ullNvtHandle; 
    ST_SOCKADDR stSrvAddr;     
} ST_TELCLT_STARTARGS, *PST_TELCLT_STARTARGS;

TELNET_CLIENT_EXT void telnet_clt_entry(void *pvParam); 

#endif

/* tcp_helper.h
 *
 * tcp协议相关的功能函数库，基于windows平台
 *
 * Neo-T, 创建于2022.05.18 09:19
 * 版本: 1.0
 *
 */
#ifndef TCPHELPER_H
#define TCPHELPER_H

#ifdef SYMBOL_GLOBALS
	#define TCPHELPER_EXT
#else
	#define TCPHELPER_EXT extern
#endif //* SYMBOL_GLOBALS

typedef CRITICAL_SECTION THMUTEX;
#define WSALIB_VER MAKEWORD(2, 2)  //* 要加载的WSA库的版本

TCPHELPER_EXT INT load_socket_lib(USHORT usVer, CHAR *pbLoadNum); 
TCPHELPER_EXT BOOL load_socket_lib(void);
TCPHELPER_EXT void unload_socket_lib(void); 
TCPHELPER_EXT SOCKET start_tcp_server(USHORT usPort, UINT unListenNum); 
TCPHELPER_EXT void stop_tcp_server(SOCKET hSocket); 
TCPHELPER_EXT SOCKET accept_client(SOCKET hSocket);
TCPHELPER_EXT USHORT crc16(const UCHAR *pubCheckData, UINT unCheckLen, USHORT usInitVal);
TCPHELPER_EXT void thread_lock_init(THMUTEX *pthMutex); 
TCPHELPER_EXT void thread_lock_uninit(THMUTEX *pthMutex);
TCPHELPER_EXT void thread_lock_enter(THMUTEX *pthMutex); 
TCPHELPER_EXT void thread_lock_leave(THMUTEX *pthMutex);
TCPHELPER_EXT void unix_time_to_local(time_t tUnixTimestamp, CHAR *pszDatetime, UINT unDatetimeBufSize);

#endif

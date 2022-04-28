/* tcp.h
 *
 * tcp协议相关功能函数
 *
 * Neo-T, 创建于2022.04.25 15:13
 * 版本: 1.0
 *
 */
#ifndef TCP_H
#define TCP_H

#ifdef SYMBOL_GLOBALS
	#define TCP_EXT
#else
	#define TCP_EXT extern
#endif //* SYMBOL_GLOBALS

TCP_EXT INT tcp_send_syn(INT nInput, in_addr_t unSrvAddr, USHORT usSrvPort);


#endif

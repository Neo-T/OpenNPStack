/* udp.h
 *
 * udp协议相关功能函数
 *
 * Neo-T, 创建于2022.06.01 15:29
 * 版本: 1.0
 *
 */
#ifndef UDP_H
#define UDP_H

#ifdef SYMBOL_GLOBALS
	#define UDP_EXT
#else
	#define UDP_EXT extern
#endif //* SYMBOL_GLOBALS
#include "ip/udp_link.h"

UDP_EXT INT udp_send_data(INT nInput, UCHAR *pubData, INT nDataLen); 

#endif

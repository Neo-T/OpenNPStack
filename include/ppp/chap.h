/*
 * 遵循Apache License 2.0开源许可协议
 *
 * 完成chap协议相关基础数据结构、宏定义，功能函数声明等相关工作
 *
 * Neo-T, 创建于2022.04.01 10:58
 *
 */
#ifndef CHAP_H
#define CHAP_H

#ifdef SYMBOL_GLOBALS
	#define CHAP_EXT
#else
	#define CHAP_EXT extern
#endif //* SYMBOL_GLOBALS

CHAP_EXT void chap_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);

#endif

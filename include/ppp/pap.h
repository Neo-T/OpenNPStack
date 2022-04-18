/* pap.h
 *
 * 完成pap协议相关基础数据结构、宏定义，功能函数声明等相关工作
 *
 * Neo-T, 创建于2022.04.02 19:05
 * 版本: 1.0
 *
 */
#ifndef PAP_H
#define PAP_H

#ifdef SYMBOL_GLOBALS
	#define PAP_EXT
#else
	#define PAP_EXT extern
#endif //* SYMBOL_GLOBALS

PAP_EXT BOOL pap_send_auth_request(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr);
PAP_EXT void pap_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);

#endif

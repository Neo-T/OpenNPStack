/* ipcp.h
 *
 * 完成ipcp协议相关基础数据结构、宏定义，功能函数声明等相关工作
 *
 * Neo-T, 创建于2022.04.03 14:01
 * 版本: 1.0
 *
 */
#ifndef IPCP_H
#define IPCP_H

#ifdef SYMBOL_GLOBALS
	#define IPCP_EXT
#else
	#define IPCP_EXT extern
#endif //* SYMBOL_GLOBALS

IPCP_EXT BOOL ipcp_send_conf_request(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr);
IPCP_EXT void ipcp_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);

#endif

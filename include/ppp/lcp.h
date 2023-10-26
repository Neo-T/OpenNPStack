/*
 * 遵循Apache License 2.0开源许可协议
 *
 * 完成lcp协议相关基础数据结构、宏定义，功能函数声明等相关工作
 *
 * Neo-T, 创建于2022.03.28 18:16
 *
 */
#ifndef LCP_H
#define LCP_H

#ifdef SYMBOL_GLOBALS
	#define LCP_EXT
#else
	#define LCP_EXT extern
#endif //* SYMBOL_GLOBALS

LCP_EXT BOOL lcp_start_negotiation(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr);
LCP_EXT void lcp_end_negotiation(PSTCB_PPP pstcbPPP); 
LCP_EXT BOOL lcp_send_conf_request(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr); 
LCP_EXT BOOL lcp_send_terminate_req(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr);
LCP_EXT BOOL lcp_send_echo_request(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr);
LCP_EXT void lcp_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);

#endif

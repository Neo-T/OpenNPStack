/*
 * 遵循Apache License 2.0开源许可协议
 *
 * 完成dhcp客户端模块相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.07.14 10:13
 *
 */
#ifndef DHCP_CLIENT_H
#define DHCP_CLIENT_H

#ifdef SYMBOL_GLOBALS
	#define DHCP_CLIENT_EXT
#else
	#define DHCP_CLIENT_EXT extern
#endif //* SYMBOL_GLOBALS

#if SUPPORT_ETHERNET
typedef struct _STCB_RENEWAL_INFO_ STCB_RENEWAL_INFO, *PSTCB_RENEWAL_INFO;
DHCP_CLIENT_EXT UCHAR *dhcp_get_option(UCHAR *pubOptions, USHORT usOptionsLen, UCHAR ubOptionCode);
DHCP_CLIENT_EXT INT dhcp_send_packet(INT nInput, PST_NETIF pstNetif, UCHAR ubOptCode, UCHAR *pubOptions, UCHAR ubOptionsLen, UINT unTransId, in_addr_t unClientIp, in_addr_t unDstIP, in_addr_t unSrcIp, EN_ONPSERR *penErr);
DHCP_CLIENT_EXT void dhcp_renewal_timeout_handler(void *pvParam);
DHCP_CLIENT_EXT void dhcp_decline(INT nInput, PST_NETIF pstNetif, UINT unTransId, in_addr_t unDeclineIp, in_addr_t unSrvIp);
DHCP_CLIENT_EXT void dhcp_timer_new(PFUN_ONESHOTTIMEOUT_HANDLER pfunTimeoutHandler, PSTCB_RENEWAL_INFO pstcbRenewalInfo, INT nTimeout);
DHCP_CLIENT_EXT BOOL dhcp_req_addr(PST_NETIF pstNetif, EN_ONPSERR *penErr); 
#endif

#endif

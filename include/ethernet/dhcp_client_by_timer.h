/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 使用one-shot-timer完成dhcp客户端模块相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.07.26 14:03
 *
 */
#ifndef DHCP_CLIENT_BY_TIMER_H
#define DHCP_CLIENT_BY_TIMER_H

#ifdef SYMBOL_GLOBALS
	#define DHCP_CLIENT_BY_TIMER_EXT
#else
	#define DHCP_CLIENT_BY_TIMER_EXT extern
#endif //* SYMBOL_GLOBALS

#if SUPPORT_ETHERNET
DHCP_CLIENT_BY_TIMER_EXT void dhcp_req_addr_timeout_handler(void *pvParam); 
#endif

#endif

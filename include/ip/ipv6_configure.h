/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 完成ipv6地址自动配置（stateless/stateful, icmpv6 + dhcpv6）
 *
 * Neo-T, 创建于2023.03.29 09:40
 *
 */
#ifndef IPv6CFG_H
#define IPv6CFG_H

#ifdef SYMBOL_GLOBALS
	#define IPv6CFG_EXT
#else
	#define IPv6CFG_EXT extern
#endif //* SYMBOL_GLOBALS

#if SUPPORT_IPV6
#if SUPPORT_ETHERNET
#define IPv6_DAD_TIMEOUT 4		//* 等待多少秒即可确定当前试探地址没有任何节点在使用
#define IPv6LNKADDR_FLAG 0x52	//* 链路本地标志，用于dad检测时区分是链路本地地址还是从路由器得到的具有生存时间的动态地址

typedef enum {
	IPv6CFG_LATENT = 0, //* 链路本地地址重复地址检测阶段
	IPv6CFG_RS     = 1, //* 路由请求阶段
	IPv6CFG_TATENT = 2, //* 链路本地地址重复地址检测阶段
	IPv6CFG_UATENT = 3, //* 链路本地地址重复地址检测阶段
	IPv6CFG_END
} EN_IPv6CFGSTATE;

IPv6CFG_EXT PST_IPv6_DYNADDR ipv6_dyn_addr_node_get(EN_ONPSERR *penErr);
IPv6CFG_EXT void ipv6_dyn_addr_node_free(PST_IPv6_DYNADDR pstDynAddrNode); 
IPv6CFG_EXT PST_IPv6_ROUTER ipv6_router_node_get(EN_ONPSERR *penErr);
IPv6CFG_EXT void ipv6_router_node_free(PST_IPv6_ROUTER pstRouterNode);
IPv6CFG_EXT PST_IPv6_DYNADDR ipv6_dyn_addr_get(CHAR bDynAddr); 
IPv6CFG_EXT PST_IPv6_ROUTER ipv6_router_get(CHAR bRouter); 

IPv6CFG_EXT BOOL ipv6_cfg_start(PST_NETIF pstNetif, EN_ONPSERR *penErr);
IPv6CFG_EXT BOOL ipv6_cfg_dad(PST_NETIF pstNetif, void *pstTentAddr, EN_ONPSERR *penErr);
#endif
#endif

#endif

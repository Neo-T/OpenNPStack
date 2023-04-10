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
#define IPv6_DAD_TIMEOUT 6		//* 等待多少秒即可确定当前试探地址没有任何节点在使用
#define IPv6LNKADDR_FLAG 0x52	//* 链路本地标志，用于dad检测时区分是链路本地地址还是从路由器得到的具有生存时间的动态地址

//* 其最大定义范围不能超过ST_IPv6::bitCfgState字段指定的位宽限制
typedef enum {
	IPv6CFG_INIT                = 0, //* 初始
	IPv6CFG_LNKADDR             = 1, //* 生成链路本地地址
	IPv6CFG_RS                  = 2, //* 路由请求
	IPv6CFG_WAIT_DYNADDRCFG_END = 3, //* 等待无状态（stateless）地址自动配置结束
	IPv6CFG_WAIT_Dv6CFG_END     = 4, //* 等待有状态和无状态Dhcpv6配置结束，如果存在Dhcpv6主机的话
	IPv6CFG_END = 5, 
} EN_IPv6CFGSTATE;

//* Ipv6地址当前状态，注意只能4个状态，否则会影响ST_IPv6_DYNAMIC::bitState或ST_IPv6_LNKLOCAL::bitState，因为其仅占据两个数据位
typedef enum {
	IPv6ADDR_TENTATIVE  = 0, //* 试探
	IPv6ADDR_PREFERRED  = 1, //* 选用
	IPv6ADDR_DEPRECATED = 2, //* 弃用
	IPv6ADDR_INVALID    = 3	 //* 无效
} EN_IPv6ADDRSTATE;

typedef enum {
	IPv6SVVTMR_INVALID = 0, //* 无效，未启动
	IPv6SVVTMR_RUN = 1,     //* 运行	
	IPv6SVVTMR_STOP = 2,    //* 结束运行，这个由ethernet_del()函数发出，通知定时器结束运行，释放相关资源，另外两个状态定时器改变
	IPv6SVVTMR_RELEASED = 3 
} EN_IPv6SVVTIMERSTATE;

//* stateful/stateless DHCPv6配置状态定义
typedef enum {
	Dv6CFG_INIT = 0, 
	Dv6CFG_SENDREQ = 1, 
	Dv6CFG_END = 2
} EN_DHCPv6CFGSTATE;

IPv6CFG_EXT PST_IPv6_DYNADDR ipv6_dyn_addr_node_get(EN_ONPSERR *penErr);
IPv6CFG_EXT void ipv6_dyn_addr_node_free(PST_IPv6_DYNADDR pstDynAddrNode); 
IPv6CFG_EXT PST_IPv6_ROUTER ipv6_router_node_get(EN_ONPSERR *penErr);
IPv6CFG_EXT void ipv6_router_node_free(PST_IPv6_ROUTER pstRouterNode);
IPv6CFG_EXT PST_IPv6_DYNADDR ipv6_dyn_addr_get(CHAR bDynAddr); 
IPv6CFG_EXT PST_IPv6_ROUTER ipv6_router_get(CHAR bRouter); 
IPv6CFG_EXT CHAR ipv6_router_get_index(PST_IPv6_ROUTER pstRouter);
IPv6CFG_EXT void netif_ipv6_dyn_addr_add(PST_NETIF pstNetif, PST_IPv6_DYNADDR pstDynAddr);
IPv6CFG_EXT void netif_ipv6_dyn_addr_del(PST_NETIF pstNetif, PST_IPv6_DYNADDR pstDynAddr);
IPv6CFG_EXT void netif_ipv6_router_add(PST_NETIF pstNetif, PST_IPv6_ROUTER pstRouter);
IPv6CFG_EXT void netif_ipv6_router_del(PST_NETIF pstNetif, PST_IPv6_ROUTER pstRouter);
IPv6CFG_EXT PST_IPv6_DYNADDR netif_ipv6_dyn_addr_next(PST_NETIF pstNetif, CHAR *pbNextAddr); 
IPv6CFG_EXT PST_IPv6_DYNADDR netif_ipv6_dyn_addr_next_safe(PST_NETIF pstNetif, PST_IPv6_DYNADDR pstPrevDynAddr, BOOL blIsRefCnt);
IPv6CFG_EXT void netif_ipv6_dyn_addr_release(PST_IPv6_DYNADDR pstDynAddr); 
IPv6CFG_EXT PST_IPv6_ROUTER netif_ipv6_router_next(PST_NETIF pstNetif, CHAR *pbNextRouter); 
IPv6CFG_EXT PST_IPv6_ROUTER netif_ipv6_router_next_safe(PST_NETIF pstNetif, PST_IPv6_ROUTER pstPrevRouter, BOOL blIsRefCnt); 
IPv6CFG_EXT void netif_ipv6_router_release(PST_IPv6_ROUTER pstRouter); 
IPv6CFG_EXT PST_IPv6_ROUTER netif_ipv6_router_get_by_addr(PST_NETIF pstNetif, UCHAR ubaRouterIpv6Addr); 

IPv6CFG_EXT BOOL ipv6_cfg_start(PST_NETIF pstNetif, EN_ONPSERR *penErr);
IPv6CFG_EXT BOOL ipv6_cfg_dad(PST_NETIF pstNetif, void *pstTentAddr, EN_ONPSERR *penErr);
#endif
#endif

#endif

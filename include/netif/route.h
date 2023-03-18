/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 路由功能相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.04.18 10:03
 *
 */
#ifndef ROUTE_H
#define ROUTE_H

#ifdef SYMBOL_GLOBALS
	#define ROUTE_EXT
#else
	#define ROUTE_EXT extern
#endif //* SYMBOL_GLOBALS
#include "netif.h"

//* Ipv4路由条目存储结构
typedef struct _ST_ROUTE_ {
    UINT unSource; 
    UINT unDestination; 
    UINT unGateway; 
    UINT unGenmask;
    PST_NETIF pstNetif; 
} ST_ROUTE, *PST_ROUTE;

//* Ipv4路由链表节点
typedef struct _ST_ROUTE_NODE_ {
    struct _ST_ROUTE_NODE_ *pstNext;
    ST_ROUTE stRoute;
} ST_ROUTE_NODE, *PST_ROUTE_NODE; 

#if SUPPORT_IPV6
//* Ipv6路由条目存储结构
typedef struct _ST_ROUTE_IPv6_ {
	UCHAR ubaSource[16];
	UCHAR ubaDestination[16];
	UCHAR ubaGateway[16];
	PST_NETIF pstNetif;
	UCHAR ubDestPrefixLen; //* 前缀长度
} ST_ROUTE_IPv6, *PST_ROUTE_IPv6; 

//* Ipv6路由链表节点
typedef struct _ST_ROUTE_IPv6_NODE_ {
	struct _ST_ROUTE_IPv6_NODE_ *pstNext;
	ST_ROUTE_IPv6 stRoute;
} ST_ROUTE_IPv6_NODE, *PST_ROUTE_IPv6_NODE;
#endif

ROUTE_EXT BOOL route_table_init(EN_ONPSERR *penErr);
ROUTE_EXT void route_table_uninit(void);
//* 参数unDestination指定目标网段地址，如果其值为0则其为缺省路由
ROUTE_EXT BOOL route_add(PST_NETIF pstNetif, UINT unDestination, UINT unGateway, UINT unGenmask, EN_ONPSERR *penErr);
ROUTE_EXT void route_del(UINT unDestination);
ROUTE_EXT void route_del_ext(PST_NETIF pstNetif);
ROUTE_EXT PST_NETIF route_get_netif(UINT unDestination, BOOL blIsForSending, in_addr_t *punSrcIp, in_addr_t *punArpDstAddr); 
ROUTE_EXT PST_NETIF route_get_default(void);
ROUTE_EXT UINT route_get_netif_ip(UINT unDestination);

#if SUPPORT_IPV6
ROUTE_EXT BOOL route_ipv6_add(PST_NETIF pstNetif, UCHAR ubaDestination[16], UCHAR ubaGateway[16], UCHAR ubDestPrefixLen, EN_ONPSERR *penErr);
ROUTE_EXT void route_ipv6_del(UCHAR ubaDestination[16]);
ROUTE_EXT void route_ipv6_del_ext(PST_NETIF pstNetif);
ROUTE_EXT PST_NETIF route_ipv6_get_netif(UCHAR ubaDestination[16], BOOL blIsForSending, UCHAR *pubSource, UCHAR *pubNSAddr); 
ROUTE_EXT PST_NETIF route_ipv6_get_default(void); 
ROUTE_EXT UCHAR *route_ipv6_get_netif_ip(UCHAR ubaDestination[16], UCHAR ubaNetifIpv6[16]);
#endif 

#endif

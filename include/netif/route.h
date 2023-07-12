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

ROUTE_EXT BOOL route_table_init(EN_ONPSERR *penErr);
ROUTE_EXT void route_table_uninit(void);
//* 参数unDestination指定目标网段地址，如果其值为0则其为缺省路由
ROUTE_EXT BOOL route_add(PST_NETIF pstNetif, UINT unDestination, UINT unGateway, UINT unGenmask, EN_ONPSERR *penErr);
ROUTE_EXT BOOL route_del(UINT unDestination, EN_ONPSERR *penErr);
ROUTE_EXT void route_del_ext(PST_NETIF pstNetif);
ROUTE_EXT PST_NETIF route_get_netif(UINT unDestination, BOOL blIsForSending, in_addr_t *punSrcIp, in_addr_t *punArpDstAddr); 
ROUTE_EXT PST_NETIF route_get_default(void);
ROUTE_EXT UINT route_get_netif_ip(UINT unDestination);

#if SUPPORT_IPV6
ROUTE_EXT PST_NETIF route_ipv6_get_netif(const UCHAR ubaDestination[16], BOOL blIsForSending, UCHAR *pubSource, UCHAR *pubNSAddr, UCHAR *pubHopLimit);
ROUTE_EXT UCHAR *route_ipv6_get_source_ip(const UCHAR ubaDestination[16], UCHAR *pubSource); //* 同route_get_netif_ip()函数，确定源地址，用于tcp/udp校验和计算伪ipv6报头
#endif  //* #if SUPPORT_IPV6

#if NETTOOLS_TELNETSRV
ROUTE_EXT const ST_ROUTE *route_get_next(const ST_ROUTE *pstNextRoute); 
#endif //* #if NETTOOLS_TELNETSRV

#endif

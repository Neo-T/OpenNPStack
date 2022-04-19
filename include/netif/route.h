/* route.h
 *
 * 路由功能相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.04.18 10:03
 * 版本: 1.0
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

//* 路由表
typedef struct _ST_ROUTE_ {
    UINT unDestination; 
    UINT unGateway; 
    UINT unGenmask;
    PST_NETIF pstNetif; 
} ST_ROUTE, *PST_ROUTE;

//* 网卡链表节点
typedef struct _ST_ROUTE_NODE_ {
    struct _ST_ROUTE_NODE_ *pstNext;
    ST_ROUTE stRoute;
} ST_ROUTE_NODE, *PST_ROUTE_NODE;

ROUTE_EXT BOOL route_table_init(EN_ONPSERR *penErr);
ROUTE_EXT void route_table_uninit(void);
//* 参数unDestination指定目标网段地址，如果其值为0则其为缺省路由
ROUTE_EXT BOOL route_add(PST_NETIF pstNetif, UINT unDestination, UINT unGateway, UINT unGenmask, EN_ONPSERR *penErr);
ROUTE_EXT void route_del(UINT unDestination);
ROUTE_EXT void route_del_ext(PST_NETIF pstNetif);
ROUTE_EXT PST_NETIF route_get_netif(UINT unDestination, BOOL blIsForSending); 

#endif

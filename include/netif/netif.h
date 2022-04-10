/* netif.h
 *
 * 网络接口（网卡）相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.04.10 10:38
 * 版本: 1.0
 *
 */
#ifndef NETIF_H
#define NETIF_H

#ifdef SYMBOL_GLOBALS
	#define NETIF_EXT
#else
	#define NETIF_EXT extern
#endif //* SYMBOL_GLOBALS
#include "protocols.h"

//* 网卡类型定义
typedef enum {
    PPP = 0, 
    ETHERNET, 
} EN_NETIF;

//* 网卡发送函数
typedef struct _ST_NETIF_ ST_NETIF, *PST_NETIF;
typedef INT(*PFUN_NETIF_SEND)(PST_NETIF pstIf, EN_NPSPROTOCOL enProtocol, SHORT sBufListHead, EN_ERROR_CODE *penErrCode);

//* 记录IPv4地址的结构体
typedef struct _ST_IPV4_ {
    UINT unAddr;
    UINT unSubnetMask;
    UINT unGateway;
    UINT unPrimaryDNS;
    UINT unSecondaryDNS;
} ST_IPV4, *PST_IPV4;

//* 存储具体网卡信息的结构体
typedef struct _ST_NETIF_ {
    EN_NETIF enType;     
    PFUN_NETIF_SEND pfunSend; 
    ST_IPV4 stIPv4;
    void *pvExtra; //* 附加信息，不同的网卡类型需要携带某些特定的信息供上层业务逻辑使用，在这里使用该字段提供访问路径
} ST_NETIF, *PST_NETIF;

//* 网卡链表节点
typedef struct _ST_NETIF_NODE_ {
    PST_NETIF pstNext; 
    ST_NETIF stIf;      
} ST_NETIF_NODE, *PST_NETIF_NODE;

NETIF_EXT BOOL netif_add(EN_NETIF enType, PST_IPV4 pstIPv4, PFUN_NETIF_SEND pfunSend, void *pvExtra, EN_ERROR_CODE *penErrCode);

#endif

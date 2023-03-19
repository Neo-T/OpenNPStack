/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 网络接口（网卡）相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.04.10 10:38
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
    NIF_UNKNOWN = 0, 
    NIF_PPP, 
    NIF_ETHERNET, 
} EN_NETIF;

//* 网卡发送函数
typedef struct _ST_NETIF_ ST_NETIF, *PST_NETIF;
typedef INT(* PFUN_NETIF_SEND)(PST_NETIF pstIf, UCHAR ubProtocol, SHORT sBufListHead, void *pvExtraParam, EN_ONPSERR *penErr); 

#if SUPPORT_ETHERNET
#define ETH_MAC_ADDR_LEN    6   //* ethernet网卡mac地址长度
typedef INT(* PFUN_EMAC_SEND)(SHORT sBufListHead, UCHAR *pubErr); 
#endif

//* 记录IPv4地址的结构体
typedef struct _ST_IPV4_ {
    UINT unAddr;
    UINT unSubnetMask;
    UINT unGateway;
    UINT unPrimaryDNS;
    UINT unSecondaryDNS;
    UINT unBroadcast; 
} ST_IPV4, *PST_IPV4;

#if SUPPORT_IPV6
typedef struct _ST_IPV6_ {
	UCHAR ubaUniAddr[16];	//* 单播地址，与PC不同的是，为了节省内存协议栈只允许一个固定Ipv6地址（手动或自动配置——自动配置先icmpv6，如失败再dhcpv6）
	UCHAR ubaTmpAddr[16];	//* 临时地址
	UCHAR ubaLnkAddr[16];	//* 链路本地地址
	UCHAR ubaGateway[16];	//* 网关地址 
	UCHAR ubUAPrefixLen;	//* 单播地址前缀长度，前缀长度的单位位：数据位，指定Ipv6地址的前多少位作为地址前缀，下同
	UCHAR ubTAPrefixLen;	//* 临时地址前缀长度
	UCHAR ubLAPrefixLen;	//* 链路本地地址前缀长度
	//UCHAR ubGAPrefixLen;	//* 网关地址前缀长度
} ST_IPV6, *PST_IPV6;
#endif

//* 存储具体网卡信息的结构体
#define NETIF_NAME_LEN  7   //* 网卡名称长度
typedef struct _ST_NETIF_ {
    EN_NETIF enType; 
    CHAR szName[NETIF_NAME_LEN];
    CHAR bUsedCount; //* 使用计数
    PFUN_NETIF_SEND pfunSend;
    ST_IPV4 stIPv4;
#if SUPPORT_IPV6
	ST_IPV6 stIPv6; 
#endif
    void *pvExtra; //* 附加信息，不同的网卡类型需要携带某些特定的信息供上层业务逻辑使用，在这里使用该字段提供访问路径
} ST_NETIF, *PST_NETIF;

//* 网卡链表节点
typedef struct _ST_NETIF_NODE_ {
    struct _ST_NETIF_NODE_ *pstNext;
    ST_NETIF stIf; 
} ST_NETIF_NODE, *PST_NETIF_NODE;

#if SUPPORT_ETHERNET
//* ethernet网卡接口附加IP地址
typedef struct _ST_NETIF_ETH_IP_NODE_ ST_NETIF_ETH_IP_NODE, *PST_NETIF_ETH_IP_NODE;
typedef struct _ST_NETIF_ETH_IP_NODE_ {
    PST_NETIF_ETH_IP_NODE pstNext;
    UINT unAddr; 
    UINT unSubnetMask; 
} ST_NETIF_ETH_IP_NODE, *PST_NETIF_ETH_IP_NODE; 

//* ethernet网卡附加信息
typedef struct _STCB_ETHARP_ STCB_ETHARP, *PSTCB_ETHARP; 
#if SUPPORT_IPV6
typedef struct _STCB_ETHIPv6MAC__ STCB_ETHIPv6MAC, *PSTCB_ETHIPv6MAC;
#endif
typedef struct _ST_NETIFEXTRA_ETH_ {     
    CHAR bIsUsed;                           //* 是否已被使用
    CHAR bIsStaticAddr;                     //* 静态地址？
    UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN];     //* mac地址   
    PST_NETIF_ETH_IP_NODE pstIPList;        //* 绑定到该网卡的IP地址
    PSTCB_ETHARP pstcbArp; 
	PSTCB_ETHIPv6MAC pstcbIpv6Mac; 
    PFUN_EMAC_SEND pfunEmacSend; 
    PST_SLINKEDLIST pstRcvedPacketList;
    HSEM hSem;
} ST_NETIFEXTRA_ETH, *PST_NETIFEXTRA_ETH;
#endif

NETIF_EXT BOOL netif_init(EN_ONPSERR *penErr);
NETIF_EXT void netif_uninit(void);
NETIF_EXT PST_NETIF_NODE netif_add(EN_NETIF enType, const CHAR *pszIfName, PST_IPV4 pstIPv4, PFUN_NETIF_SEND pfunSend, void *pvExtra, EN_ONPSERR *penErr); 
NETIF_EXT void netif_del(PST_NETIF_NODE pstNode); 
NETIF_EXT void netif_del_ext(PST_NETIF pstNetif);
NETIF_EXT PST_NETIF netif_get_first(BOOL blIsForSending);
NETIF_EXT PST_NETIF netif_get_by_ip(UINT unNetifIp, BOOL blIsForSending); 
NETIF_EXT PST_NETIF netif_get_by_name(const CHAR *pszIfName); 
#if SUPPORT_ETHERNET
NETIF_EXT PST_NETIF netif_get_eth_by_genmask(UINT unDstIp, in_addr_t *punSrcIp, BOOL blIsForSending);
#endif
NETIF_EXT UINT netif_get_first_ip(void);
NETIF_EXT void netif_used(PST_NETIF pstNetif);
NETIF_EXT void netif_freed(PST_NETIF pstNetif);
NETIF_EXT BOOL netif_is_ready(const CHAR *pszIfName); 
NETIF_EXT UINT netif_get_source_ip_by_gateway(PST_NETIF pstNetif, UINT unGateway);

#if SUPPORT_IPV6
NETIF_EXT UCHAR *netif_get_source_ipv6_by_destination(PST_NETIF pstNetif, UCHAR ubaDestination[16]); 
#if SUPPORT_ETHERNET
NETIF_EXT PST_NETIF netif_get_eth_by_ipv6_prefix(UCHAR ubaDestination[16], UCHAR *pubSource, BOOL blIsForSending);
#endif
#endif

#endif

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
//* Ipv6地址当前状态，注意只能4个状态，否则会影响ST_IPv6_DYNADDR::bitState或ST_IPv6_LNKADDR::bitState，因为其仅占据两个数据位
typedef enum {
    IPv6ADDR_TENTATIVE  = 0, //* 试探
    IPv6ADDR_PREFERRED  = 1, //* 选用
    IPv6ADDR_DEPRECATED = 2, //* 弃用
    IPv6ADDR_INVALID    = 3	 //* 无效
} EN_IPv6ADDRSTATE;
#define ipv6_addr_state(enIpv6AddrState) (enIpv6AddrState ? ((enIpv6AddrState == 1) ? "Preferred" : ((enIpv6AddrState == 2) ? "Deprecated" : "Invalid")) : "Tentative")

//* 动态生成的IPv6地址（无状态/有状态地址自动配置生成的ipv6地址）,这种地址其前缀由路由器或dhcpv6服务器分配，具有时效性，其并不固定
PACKED_BEGIN
typedef struct _ST_IPv6_DYNADDR_ { 
	UCHAR ubaVal[16];			//* 必须放在结构体的首部，因为其还承担着dad检测标识地址类型的任务，其最后一个字节为0标识这是动态地
	                            //* 址，为IPv6LNKADDR_FLAG值（参见ipv6_configure.h文件）则代表这是链路本地地址

	USHORT bitState        : 2; //* 当前状态
	USHORT bitConflict     : 1; //* 是否收到地址冲突报文	
	USHORT bitOptCnt       : 3;	//* 操作计数
	USHORT bitRouter       : 3; //* 通过哪个路由器通告得到的这个地址，其为访问这个路由器相关配置信息的索引值（协议栈最多支持8个路由器）
	USHORT bitPrefixBitLen : 7;	//* 前缀长度
	UINT unValidLifetime;		//* 有效生存时间，单位：秒，全1表示无限长，否则到期则地址失效，将不再使用

	UINT unPreferredLifetime;	//* 推荐给节点选用的生存时间，单位：秒，全1表示无限长。这个时间小于等于有效生存时间，其生存时间段内该地
	                            //* 址可建立新的连接，到期后则只能维持现有连接不再建立新的连接，有效生存时间到期则现有连接亦无效，该地址
	                            //* 将被释放结束使用

	CHAR bNextAddr;				//* 指向下一个ipv6动态地址
} PACKED ST_IPv6_DYNADDR, *PST_IPv6_DYNADDR;
PACKED_END
#define i6a_ref_cnt bitOptCnt

//* 链路本地地址
PACKED_BEGIN
typedef struct _ST_IPv6_LNKADDR_ {
	UCHAR ubaVal[16];		//* 地址组成形式为：FE80::/64 + EUI-64地址，参见icmpv6.c文件icmpv6_lnk_addr()函数实现，注意，同ST_IPv6_DYNADDR必须放在首部，目的与之相同
	UCHAR bitState     : 2;	//* 链路本地地址当前状态
	UCHAR bitConflict  : 1; //* 是否收到地址冲突报文
	UCHAR bitOptCnt    : 3;	//* 操作计数
	UCHAR bitReserved  : 2; //* 保留
} PACKED ST_IPv6_LNKADDR, *PST_IPv6_LNKADDR;
PACKED_END

//* 路由器，路由器路由器优先级进行了重新定义，方便程序比较
#define IPv6ROUTER_PRF_LOW    0 //* 自定义的路由器优先级：低
#define IPv6ROUTER_PRF_MEDIUM 1 //* 自定义的路由器优先级：中
#define IPv6ROUTER_PRF_HIGH   2 //* 自定义的路由器优先级：高
#define i6r_prf_converter(router_prf) ((router_prf) ? ((router_prf) > 1 ? 0 : 2) : 1)
#define i6r_prf_desc(router_prf)      ((router_prf) ? ((router_prf) > 1 ? "High" : "Medium") : "Low")
PACKED_BEGIN
typedef struct _ST_IPv6_ROUTER_ { 
	UCHAR ubaAddr[16]; //* 路由器地址	
	UCHAR ubHopLimit;  //* 路由器给出的跳数限制

	UCHAR bitRefCnt      : 3; //* 引用计数
	UCHAR bitManaged     : 1; //* 路由器RA报文携带的M标志（Managed Flag, Managed address configuration）,复位意味着RA报文的O标志置位（Other Flag，Other Configuration）
	UCHAR bitDv6CfgState : 1; //* stateful/stateless DHCPv6配置状态
	UCHAR bitReserved    : 1; //* 保留
	UCHAR bitPrf         : 2; //* 默认路由器优先级，01：高；00：中；11；低；10，未指定，发送端不应该发送此值，收到则视作00处理	

	USHORT usLifetime; //* 路由器生存时间，如果为0则其不能作为默认路由器，也就是默认网关

	struct { //* dns服务器，支持主从两个地址
		UCHAR ubaAddr[16]; //* 地址
		UINT unLifetime;   //* 生存时间
	} PACKED staDNSSrv[2];    

	USHORT usMtu; 
	//UCHAR ubaMacAddr[6];
	PST_NETIF pstNetif; 
	CHAR bDv6Client;  //* 指向DHCPv6客户端控制块的指针
	CHAR bNextRouter; //* 指向下一个路由器
} PACKED ST_IPv6_ROUTER, *PST_IPv6_ROUTER;
PACKED_END
#define i6r_flag_prf        bitPrf
#define i6r_flag_m          bitManaged
//#define i6r_ref_cnt_mask	0x07 //* ST_IPv6_ROUTER::uniFlag::stb8::bitReserved的位宽
//#define i6r_flag_mask		0xF8 //* 8 - ST_IPv6_ROUTER::uniFlag::stb8::bitReserved的位宽
#define i6r_ref_cnt         bitRefCnt

PACKED_BEGIN
typedef struct _ST_IPv6_ {	
	ST_IPv6_LNKADDR stLnkAddr; //* 链路本地地址
	CHAR bDynAddr;			   //* 自动配置生成的拥有生存时间限制的动态ipv6地址
	CHAR bRouter;              //* 通过RA或DHCPv6获得的链路内可用的路由器，这里比ST_IPv6_DYNADDR::bitRouter多一位的目的是利用第4位来标识其是否已挂接一个有效路由器节点，即bitRouter > 7 代表尚未挂接任何路由器	
	CHAR bitCfgState      : 3; //* 地址配置状态
	CHAR bitOptCnt        : 3; //* 操作计数
	CHAR bitSvvTimerState : 2; //* 生存计时器状态（Survival timer）
	//PST_ONESHOTTIMER pstTimer;	//* 用于地址配置的one-shot定时器，完成周期性定时操作
} PACKED ST_IPv6, *PST_IPv6; 
PACKED_END
#define nif_lla_ipv6 stIPv6.stLnkAddr.ubaVal
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
	ST_IPv6 stIPv6; 
#endif
    void *pvExtra; //* 附加信息，不同的网卡类型需要携带某些特定的信息供上层业务逻辑使用，在这里使用该字段提供访问路径
} ST_NETIF, *PST_NETIF;

//* 网卡链表节点
typedef struct _ST_NETIF_NODE_ {
    struct _ST_NETIF_NODE_ *pstNext;
    ST_NETIF stIf; 
} ST_NETIF_NODE, *PST_NETIF_NODE;

#if SUPPORT_ETHERNET
#define ETH_MAC_ADDR_LEN  6   //* ethernet网卡mac地址长度

#if ETH_EXTRA_IP_EN
//* ethernet网卡接口附加IP地址
typedef struct _ST_ETH_EXTRA_IP_ { 
    UINT unAddr; 
    UINT unSubnetMask; 
} ST_ETH_EXTRA_IP, *PST_ETH_EXTRA_IP;
#endif //* #if ETH_EXTRA_IP_EN

//* ethernet网卡附加信息
typedef struct _STCB_ETHARP_ STCB_ETHARP, *PSTCB_ETHARP; 
#if SUPPORT_IPV6
typedef struct _STCB_ETHIPv6MAC__ STCB_ETHIPv6MAC, *PSTCB_ETHIPv6MAC;
#endif
typedef struct _ST_NETIFEXTRA_ETH_ {     
    CHAR bIsUsed;                           //* 是否已被使用
    CHAR bIsStaticAddr;                     //* 静态地址？
    UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN];     //* mac地址   
#if ETH_EXTRA_IP_EN
    ST_ETH_EXTRA_IP staExtraIp[ETH_EXTRA_IP_NUM]; //* 绑定到该网卡的附加IP地址
#endif
    PSTCB_ETHARP pstcbArp; 
#if SUPPORT_IPV6
	PSTCB_ETHIPv6MAC pstcbIpv6Mac; 
#endif
    PFUN_EMAC_SEND pfunEmacSend; 
    PST_SLINKEDLIST pstRcvedPacketList;
    HSEM hSem;
} ST_NETIFEXTRA_ETH, *PST_NETIFEXTRA_ETH;
#endif

NETIF_EXT BOOL netif_init(EN_ONPSERR *penErr);
NETIF_EXT void netif_uninit(void);
NETIF_EXT PST_NETIF_NODE netif_add(EN_NETIF enType, const CHAR *pszIfName, PST_IPV4 pstIPv4, PFUN_NETIF_SEND pfunSend, void *pvExtra, EN_ONPSERR *penErr); 
NETIF_EXT void netif_set_default(PST_NETIF pstNetif); 
NETIF_EXT void netif_del(PST_NETIF_NODE pstNode); 
NETIF_EXT void netif_del_ext(PST_NETIF pstNetif);
NETIF_EXT PST_NETIF netif_get_first(BOOL blIsForSending);
NETIF_EXT PST_NETIF netif_get_by_ip(UINT unNetifIp, BOOL blIsForSending); 
NETIF_EXT PST_NETIF netif_get_by_name(const CHAR *pszIfName); 
#if SUPPORT_ETHERNET
NETIF_EXT PST_NETIF netif_eth_get_by_genmask(UINT unDstIp, in_addr_t *punSrcIp, BOOL blIsForSending); 
NETIF_EXT void netif_eth_set_ip(PST_NETIF pstNetif, in_addr_t unIp, in_addr_t unSubnetMask, in_addr_t unGateway);
#if ETH_EXTRA_IP_EN
NETIF_EXT BOOL netif_eth_add_ip(PST_NETIF pstNetif, in_addr_t unIp, in_addr_t unSubnetMask, EN_ONPSERR *penErr);
NETIF_EXT void netif_eth_del_ip(PST_NETIF pstNetif, in_addr_t unIp);
NETIF_EXT BOOL netif_eth_add_ip_by_if_name(const CHAR *pszIfName, in_addr_t unIp, in_addr_t unSubnetMask, EN_ONPSERR *penErr); 
NETIF_EXT BOOL netif_eth_del_ip_by_if_name(const CHAR *pszIfName, in_addr_t unIp, EN_ONPSERR *penErr);
#endif //* #if ETH_EXTRA_IP_EN
#endif
NETIF_EXT UINT netif_get_first_ip(void);
NETIF_EXT void netif_used(PST_NETIF pstNetif);
NETIF_EXT void netif_freed(PST_NETIF pstNetif);
NETIF_EXT BOOL netif_is_ready(const CHAR *pszIfName); 
NETIF_EXT UINT netif_get_source_ip_by_gateway(PST_NETIF pstNetif, UINT unGateway);

#if SUPPORT_IPV6
NETIF_EXT const UCHAR *ipv6_get_loopback_addr(void);
#if SUPPORT_ETHERNET
NETIF_EXT PST_NETIF netif_eth_get_by_ipv6_prefix(const UCHAR ubaDestination[16], UCHAR *pubSource, UCHAR *pubNSAddr, BOOL blIsForSending, UCHAR *pubHopLimit); 
#if NETTOOLS_TELNETSRV
NETIF_EXT UCHAR *netif_eth_get_next_ipv6(const ST_NETIF *pstNetif, UCHAR ubaNextIpv6[16], EN_IPv6ADDRSTATE *penState, UINT *punValidLifetime); 
NETIF_EXT UCHAR *netif_eth_get_next_ipv6_router(const ST_NETIF *pstNetif, UCHAR ubaNextRouterAddr[16], CHAR *pbRouterPrf, USHORT *pusMtu, USHORT *pusLifetime, UCHAR ubaPriDnsAddr[16], UCHAR ubaSecDnsAddr[16]); 
#endif //* #if NETTOOLS_TELNETSRV
#endif
#endif

#if NETTOOLS_TELNETSRV
#if NVTCMD_TELNET_EN
NETIF_EXT BOOL is_local_ip(in_addr_t unAddr); 
#endif //* #if NVTCMD_TELNET_EN
NETIF_EXT const ST_NETIF *netif_get_next(const ST_NETIF *pstNextNetif); 
#if SUPPORT_ETHERNET
NETIF_EXT CHAR *netif_eth_mac_to_ascii(const UCHAR *pubMac, CHAR *pszMac); 
NETIF_EXT UCHAR *netif_eth_ascii_to_mac(const CHAR *pszMac, UCHAR *pubMac);
NETIF_EXT BOOL netif_eth_set_ip_by_if_name(const CHAR *pszIfName, in_addr_t unIp, in_addr_t unSubnetMask, in_addr_t unGateway, CHAR *pbIsStaticAddr, EN_ONPSERR *penErr);
NETIF_EXT BOOL netif_eth_set_mac_by_if_name(const CHAR *pszIfName, const CHAR *pszMac, EN_ONPSERR *penErr); //* 参数pszMac指向类似4E-65-XX-XX-XX-XX格式的可读字符串
NETIF_EXT BOOL netif_eth_set_dns_by_if_name(const CHAR *pszIfName, in_addr_t unPrimaryDns, in_addr_t unSecondaryDns, EN_ONPSERR *penErr); 
NETIF_EXT BOOL netif_eth_is_static_addr(const CHAR *pszIfName, EN_ONPSERR *penErr);
#if ETH_EXTRA_IP_EN
NETIF_EXT UINT netif_eth_get_next_ip(const ST_NETIF *pstNetif, UINT *punSubnetMask, UINT unNextIp); 
#endif //* #if ETH_EXTRA_IP_EN
#endif //* #if SUPPORT_ETHERNET
#endif //* #if NETTOOLS_TELNETSRV
#endif

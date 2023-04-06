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
//typedef struct _ST_ONESHOTTIMER_ ST_ONESHOTTIMER, *PST_ONESHOTTIMER; 

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
	UCHAR ubaVal[16];		//* 地址组成形式为：FE80::/64 + EUI-64地址，参见icmpv6.c文件icmpv6_lnk_addr_get()函数实现，注意，同ST_IPv6_DYNADDR必须放在首部，目的与之相同
	UCHAR bitState     : 2;	//* 链路本地地址当前状态
	UCHAR bitConflict  : 1; //* 是否收到地址冲突报文
	UCHAR bitOptCnt    : 3;	//* 操作计数
	UCHAR bitReserved  : 2; //* 保留
} PACKED ST_IPv6_LNKADDR, *PST_IPv6_LNKADDR;
PACKED_END

//* 路由器
PACKED_BEGIN
typedef struct _ST_IPv6_ROUTER_ { 
	UCHAR ubaAddr[16];			//* 路由器地址	
	UCHAR ubHopLimit;			//* 路由器给出的跳数限制
	union {
		struct { //* 为了方便操作，位序与icmpv6中的RA报文完全一致，参见icmpv6_frame.h中ST_ICMPv6_RA_HDR结构体定义			
			UCHAR bitReserved : 3; //* 保留，为了节省内存在这里用作引用计数

			UCHAR bitPrf      : 2; //* 默认路由器优先级，01：高；00：中；11低；10，强制路由器生存时间字段值为0，发出通告的路由器不能
			                       //* 成为默认路由器。优先级字段用于有两台路由器的子网环境，主辅路由器互为备份（主无法使用是辅上）

			UCHAR bitAgent    : 1; //* RFC 3775为移动ipv6准备

			UCHAR bitOther    : 1; //* Other Configuration，O标志，当M标志为0时该位才会被启用，也就是此时程序才会去关注这个标志。当其置位，且
			                       //* icmpv6 option - Prefix information中A标志置位则协议栈将通过DHCPv6获得其它参数，否则不通过DHCPv6获得其它参数

			UCHAR bitManaged  : 1; //* Managed address configuration，M标志，指示是否配置有状态ipv6地址。置位：无状态配置结束后可以通过DHCPv6进行
			                       //* 地址配置（获得的ipv6地址及dns等）；反之则不支持通过DHCPv6进行地址配置
		} PACKED stb8; 
		UCHAR ubVal; 
	} PACKED uniFlag; 

	USHORT usLifetime; //* 路由器生存时间，如果为0则其不能作为默认路由器，也就是默认网关

	struct { //* 主dns服务器
		UCHAR ubaAddr[16]; //* 地址
		UINT unLifetime;	   //* 生存时间
	} PACKED stMasterDNSSrv; 

	struct { //* 从dns服务器
		UCHAR ubaAddr[16]; 
		UINT unLifetime;
	} PACKED stSlaveDNSSrv;    

	USHORT usMtu; 
	UCHAR ubaMacAddr[6];
	PST_NETIF pstNetif; 
	CHAR bNextRouter; //* 指向下一个路由器
} PACKED ST_IPv6_ROUTER, *PST_IPv6_ROUTER;
PACKED_END
#define i6r_flag_prf		uniFlag.stb8.bitPrf
#define i6r_flag_a			uniFlag.stb8.bitAgent
#define i6r_flag_o			uniFlag.stb8.bitOther
#define i6r_flag_m			uniFlag.stb8.bitManaged
#define i6r_flag			uniFlag.ubVal
#define i6r_ref_cnt_mask	0x07 //* ST_IPv6_ROUTER::uniFlag::stb8::bitReserved的位宽
#define i6r_flag_mask		0xF8 //* 8 - ST_IPv6_ROUTER::uniFlag::stb8::bitReserved的位宽
#define i6r_ref_cnt			uniFlag.stb8.bitReserved 

PACKED_BEGIN
typedef struct _ST_IPv6_ {	
	ST_IPv6_LNKADDR stLnkAddr; //* 链路本地地址
	CHAR bDynAddr;			   //* 自动配置生成的拥有生存时间限制的动态ipv6地址
	CHAR bRouter;			   //* 通过RA或DHCPv6获得的链路内可用的路由器
	CHAR bitCfgState      : 4; //* 地址配置状态
	CHAR bitSvvTimerState : 2; //* 生存计时器状态
	CHAR bitReserved      : 2; //* 保留
	//PST_ONESHOTTIMER pstTimer;	//* 用于地址配置的one-shot定时器，完成周期性定时操作
} PACKED ST_IPv6, *PST_IPv6; 
PACKED_END
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

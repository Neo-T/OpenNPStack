/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * DHCPv6协议相关功能函数
 *
 * Neo-T, 创建于2023.04.14 20:34
 *
 */
#ifndef DHCPv6_H
#define DHCPv6_H

#ifdef SYMBOL_GLOBALS
	#define DHCPv6_EXT
#else
	#define DHCPv6_EXT extern
#endif //* SYMBOL_GLOBALS
#include "dhcpv6_frame.h"

#define Dv6CFGADDR_PREFIX_LEN 124 //* DHCPv6服务器分配地地址无法获得前缀长度，所以为了路由选择及标识这是DHCPv6分配地地址，将其前缀长度设为最大

#if SUPPORT_IPV6 && SUPPORT_ETHERNET
typedef enum  {
	Dv6CLT_SOLICIT = 0, 
	Dv6CLT_REQUEST, 	
	Dv6CLT_RENEW, 
	Dv6CLT_REBIND, 
	Dv6CLT_RELEASE, 
	Dv6CLT_RESTART
} EN_DHCPv6CLTSTATE;

#define DUID_SRVID_LEN_MAX 15 //* Server Identifier Option携带的Server Id为DUID_EN类型时允许的最大企业标识长度

PACKED_BEGIN
typedef struct _STCB_DHCPv6_CLIENT_ {
	INT nInput;               //* DHCPv6客户端句柄	
	UINT unStartTimingCounts; //* 时间计数
	UINT unTransId;           //* 事务Id
	UINT unT1;                //* 参见ST_DHCPv6OPT_IANA_HDR结构体针对该字段的定义
	UINT unT2;                //* 同上
	struct {
		UCHAR ubSrvIdLen; //* Server Identifier长度
		UCHAR ubaVal[DUID_SRVID_LEN_MAX]; //* 服务器Id 
	} PACKED stSrvId;
	USHORT usStatusCode;   //* 状态码
	//UCHAR ubaSrvAddr[16];  //* 单播通讯时DHCPv6服务器地址
	UCHAR ubaIAAddr[16];   //* 服务器分配的地址
	UCHAR bitState    : 3; //* 客户端当前工作状态，具体状态定义参见EN_DHCPv6CLTSTATE
	UCHAR bitUnicast  : 1; //* 单播标记，置位则意味着收到服务器的DHCPv6OPT_UNICAST选项，此后的通讯均直接点对点发送而不是继续发送组播
	UCHAR bitRcvReply : 1; //* 收到回复
	UCHAR bitOptCnt   : 3; //* 操作计数
	CHAR bDynAddr;         //* 配置的地址节点，如果其小于0，则意味着这个路由器仅支持获取除地址之外的其它配置参数（如DNS服务器等）
	CHAR bRouter;          //* 发起dhcp配置的路由器	 
	CHAR bNext;            //* 指向下一个客户端
} PACKED STCB_DHCPv6_CLIENT, *PSTCB_DHCPv6_CLIENT;
PACKED_END

DHCPv6_EXT void dhcpv6_client_ctl_block_init(void); 
DHCPv6_EXT PSTCB_DHCPv6_CLIENT dhcpv6_client_node_get(CHAR *pbNodeIdx, EN_ONPSERR *penErr);
DHCPv6_EXT void dhcpv6_client_node_free(PSTCB_DHCPv6_CLIENT pstClientNode); 
DHCPv6_EXT PSTCB_DHCPv6_CLIENT dhcpv6_client_get(CHAR bClient);
DHCPv6_EXT CHAR dhcpv6_client_get_index(PSTCB_DHCPv6_CLIENT pstClient);
DHCPv6_EXT PSTCB_DHCPv6_CLIENT dhcpv6_client_find_by_ipv6(PST_NETIF pstNetif, UCHAR ubaRouterAddr[16], PST_IPv6_ROUTER *ppstRouter);

DHCPv6_EXT INT dhcpv6_client_start(PST_IPv6_ROUTER pstRouter, EN_ONPSERR *penErr); 
DHCPv6_EXT void dhcpv6_client_stop(PSTCB_DHCPv6_CLIENT pstClient);
DHCPv6_EXT INT dhcpv6_send_solicit(PSTCB_DHCPv6_CLIENT pstClient, PST_IPv6_ROUTER pstRouter, EN_ONPSERR *penErr);
DHCPv6_EXT INT dhcpv6_send_request(PSTCB_DHCPv6_CLIENT pstClient, PST_IPv6_ROUTER pstRouter, USHORT usMsgType);
DHCPv6_EXT void dhcpv6_recv(PST_NETIF pstNetif, UCHAR ubaSrcAddr[16], UCHAR ubaDstAddr[16], UCHAR *pubDHCPv6, USHORT usDHCPv6Len);
#endif
#endif

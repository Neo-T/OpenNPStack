/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * DHCPv6帧结构定义，DHCPv6协议说明参见[RFC3315]以及18年的更新版[RFC8415]
 * 2003版：https://www.rfc-editor.org/rfc/rfc3315
 * 2018版：https://www.rfc-editor.org/rfc/rfc8415.html
 *
 * Neo-T, 创建于2023.04.14 20:36
 *
 */
#ifndef DHCPv6_FRAME_H
#define DHCPv6_FRAME_H

#if SUPPORT_IPV6 && SUPPORT_ETHERNET

 //* 协议栈支持的DHCPv6消息类型定义，详细、完整的消息类型参见：https://www.iana.org/assignments/dhcpv6-parameters/dhcpv6-parameters.xhtml#dhcpv6-parameters-1
 //* 以下各消息类型的详细说明参见[RFC8415]7.3节：https://www.rfc-editor.org/rfc/rfc8415.html#section-7.3
#define DHCPv6MSGTYPE_SOLICIT     1  //* 客户端发送请求消息以定位DHCPv6服务器
#define DHCPv6MSGTYPE_ADVERTISE   2  //* DHCPv6服务器回应请求（SOLICIT）消息
#define DHCPv6MSGTYPE_REQUEST     3  //* 客户端发送请求消息以获取诸如地址、dns等配置参数
#define DHCPv6MSGTYPE_CONFIRM     4  //* 当ipv6链路发生变化时（比如客户端重启或网线断开了）客户端发送确认消息以期确定当前分配的地址是否仍然可以继续使用
#define DHCPv6MSGTYPE_RENEW       5  //* 客户端发送续租消息以期继续使用先前服务器分配的地址并更新dns等其它配置参数
#define DHCPv6MSGTYPE_REBIND      6  //* 同上，只不过这个是在没有收到续租消息的应答后发送的
#define DHCPv6MSGTYPE_REPLY       7  //* 服务器的应答消息，响应客户端发送的Solicit、Request、Renew、Rebind四类消息
#define DHCPv6MSGTYPE_RELEASE     8  //* 结束租期，归还地址资源给服务器
#define DHCPv6MSGTYPE_DECLINE     9  //* 客户端通知服务器拒绝当前分配的地址，因为链路上已经有客户在使用这个地址
#define DHCPv6MSGTYPE_RECONFIGURE 10 //* 服务器通知客户端有新的或修改过的配置参数需要客户端重新发送Renew、Rebind或Information-request消息重新获取
#define DHCPv6MSGTYPE_INFOREQUEST 11 //* 客户端请求除地址租约之外的其它配置参数

//* 协议栈支持的DHCPv6选项定义，完整的选项列表参见：https://www.iana.org/assignments/dhcpv6-parameters/dhcpv6-parameters.xhtml#dhcpv6-parameters-2
//* 这里定义的选项并不涵盖IANA持续维护、更新的全部选项，仅包括实现ipv6地址自动配置相关的选项，这些选项均在[RFC8415]21节给出：
//* https://www.rfc-editor.org/rfc/rfc8415.html#section-21
#define DHCPv6OPT_CLTID 1  //* Client Identifier Option，客户端ID
#define DHCPv6OPT_SRVID 2  //* Server Identifier Option，服务器ID
#define DHCPv6OPT_IANA  3  //* Identity Association for Non-temporary Addresses Option，用于携带非临时地址及与该地址关联的相关配置参数的选项
#define DHCPv6OPT_IATA  4  //* Identity Association for Temporary Addresses Option，用于携带临时地址及与该地址关联的相关配置参数的选项
#define DHCPv6OPT_IAA   5  //* IA Address Option，与IANA或IATA选项关联的地址选项，其必须作为子选项被封装在这两个选项中，参见ST_DHCPv6OPT_IANA_HDR及ST_DHCPv6OPT_IATA_HDR定义
#define DHCPv6OPT_SCODE 13 //* Status Code Option，与IAA等需要指示操作结果的选项关联的选项，其应当作为子选项包含在DHCP的选项中，参见ST_DHCPv6OPT_IAA_HDR定义


//* DHCPv6帧头部结构体
PACKED_BEGIN
typedef struct _UNI_DHCPv6_HDR_ {
	struct {
		UINT bitTransId : 24; //* Transaction ID，唯一标识本次dhcp请求的标识符，注意，本次请求所有报文标识符均一致
		UINT bitMsgType : 8;  //* Message Type，消息类型，详见[RFC8415]7.3节：https://www.rfc-editor.org/rfc/rfc8415.html#section-7.3
	} PACKED stb32;
	UINT unVal;
} PACKED UNI_DHCPv6_HDR, *PUNI_DHCPv6_HDR;
PACKED_END

//* DHCPv6选项头部结构
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_HDR_ {
	USHORT usCode;    //* 选项类型
	USHORT usDataLen; //* 选项携带的数据长度（注意这个长度不包括ST_DHCPv6_OPT_HDR长度，就是纯粹的数据长度，不同的选项长度不同）
} PACKED ST_DHCPv6OPT_HDR, *PST_DHCPv6OPT_HDR;
PACKED_END

//* DHCPv6选项-Client/Server Identifier Option，根据[RFC8415]11.1节描述，客户端/服务器ID（DUID，DHCP Unique Identifier）共四种类型，详见：https://www.rfc-editor.org/rfc/rfc8415.html#section-11.1 
//* DUID类型定义如下：
#define DUID_LLT  1 //* DUID Based on Link-Layer Address Plus Time
#define DUID_EN   2 //* DUID Assigned by Vendor Based on Enterprise Number
#define DUID_LL   3 //* DUID Based on Link-Layer Address (DUID-LL)
#define DUID_UUID 4 //* DUID Based on Universally Unique Identifier

//* 硬件类型（Hardware Type）定义，详细、完整的类型定义参见：https://www.iana.org/assignments/arp-parameters/arp-parameters.xhtml#arp-parameters-2
//* 这里仅定义协议栈现在以及将来计划支持的硬件类型
#define DHW_ETHERNET 1 //* Ethernet
#define DHW_IEEE802  6 //* IEEE 802 Networks
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_DUID_HDR_ {
	ST_DHCPv6OPT_HDR stHdr;
	USHORT usType;         //* 指定DUID类型
	USHORT usHardwareType; //* 硬件类型
	/* …… */ //* 可变长度的标识
} PACKED ST_DHCPv6OPT_DUID_HDR, *PST_DHCPv6OPT_DUID_HDR;
PACKED_END

//* DHCPv6选项-Identity Association for Non-temporary Addresses Option
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_IANA_HDR_ {
	ST_DHCPv6OPT_HDR stHdr;
	UINT unId; //* IA标识
	UINT unT1; //* 指定发出第一次续租（renew）请求的时间，单位：秒
	UINT unT2; //* 指定第一次续租请求不成功后，发送rebind请求继续续租的时间，注意，T2包含T1，所以renew失败后发送rebind的实际间隔
	           //* 应为T2 - T1（与DHCPv4完全相同，其实T1为0.5倍租期，T2为0.8倍租期）单位：秒
	/* …… */ //* IANA选项，变长，通过选项IAA携带的租用给客户端的Ipv6地址，注意一个IANA/IATA可能携带多个IAA选项（多个地址）
} PACKED ST_DHCPv6OPT_IANA_HDR, *PST_DHCPv6OPT_IANA_HDR;
PACKED_END

//* DHCPv6选项-Identity Association for Temporary Addresses Option
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_IATA_HDR_ {
	ST_DHCPv6OPT_HDR stHdr;
	UINT unId; //* IA标识				   
	/* …… */ //* IANA选项，变长，通过选项IAA携带的临时Ipv6地址，同样其可能携带多个IAA选项（多个地址）
} PACKED ST_DHCPv6OPT_IANA_HDR, *PST_DHCPv6OPT_IANA_HDR;
PACKED_END

//* DHCPv6选项-IA Address Option，IANA与IATA的子选项，携带出租/租用的地址或请求/分配的临时地址
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_IAA_HDR_ {
	ST_DHCPv6OPT_HDR stHdr;
	UCHAR ubaIpv6Addr[16];    //* 出租/租用的地址或请求/分配的临时地址，根据[RFC8415]24.6节关于"IPv6-address"字段的描述，这个地址其前缀128位长
	UINT unPreferredLifetime; //* 推荐给节点选用的生存时间，单位：秒
	UINT unValidLifetime;     //* 有效生存时间，单位：秒，全1表示无限长，否则到期则地址失效，将不再使用，应谨慎使用
			   /* …… */ //* 可能会存在IAaddr-options字段，如果该IA地址存在操作错误，则IAA选项会携带SCODE子选项指示当前操作结果
} PACKED ST_DHCPv6OPT_IAA_HDR, *PST_DHCPv6OPT_IAA_HDR;
PACKED_END

//* DHCPv6选项-Status Code Option，IAA等需要指示操作结果的选项的子选项，其包含在这些选项中
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_SCODE_HDR_ {
	ST_DHCPv6OPT_HDR stHdr;
	USHORT usCode; //* 状态码，详细完整且处于持续更新状态的状态码定义参见：https://www.iana.org/assignments/dhcpv6-parameters/dhcpv6-parameters.xhtml#dhcpv6-parameters-5
	               //* 协议栈用到的状态码参见[RFC8415]21.13节“Table 3: Status Code Definitions”：https://www.rfc-editor.org/rfc/rfc8415.html#section-21.13

	/* …… */ //* 可变长度的utf8编码的给终端用户显式的状态消息（status-message）
} PACKED ST_DHCPv6OPT_SCODE_HDR, *PST_DHCPv6OPT_SCODE_HDR;
PACKED_END

#endif
#endif

/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2023.04.14 20:36
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * DHCPv6帧结构定义，DHCPv6协议说明参见[RFC3315]以及18年的更新版[RFC8415]
 * 2003版：https://www.rfc-editor.org/rfc/rfc3315
 * 2018版：https://www.rfc-editor.org/rfc/rfc8415.html
 *
 */
#ifndef DHCPv6_FRAME_H
#define DHCPv6_FRAME_H

#if SUPPORT_IPV6 && SUPPORT_ETHERNET

#define DHCPv6_SRV_PORT 547  //* DHCPv6服务器端口
#define DHCPv6_CLT_PORT 546  //* DHCPv6客户端端口

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
#define DHCPv6OPT_CLTID         1  //* Client Identifier Option，客户端ID
#define DHCPv6OPT_SRVID         2  //* Server Identifier Option，服务器ID
#define DHCPv6OPT_IANA          3  //* Identity Association for Non-temporary Addresses Option，用于携带非临时地址及与该地址关联的相关配置参数的选项
#define DHCPv6OPT_IATA          4  //* Identity Association for Temporary Addresses Option，用于携带临时地址及与该地址关联的相关配置参数的选项
#define DHCPv6OPT_IAA           5  //* IA Address Option，与IANA或IATA选项关联的地址选项，其必须作为子选项被封装在这两个选项中，参见ST_DHCPv6OPT_IANA_HDR及ST_DHCPv6OPT_IATA_HDR定义
#define DHCPv6OPT_ORO           6  //* Option Request Option，Solicit、Request、Renew、Rebind、information-request这几个消息类型必须携带ORO（Option Request Option)以告知服务器客户端想要那几个选项
#define DHCPv6OPT_PRF           7  //* Preference Option，服务器通过Advertise消息携带这个选项，指定服务器的优先级，值大者胜出
#define DHCPv6OPT_ETIME         8  //* Elapsed Time Option，用于计量完成整个DHCPv6交互的时间，单位：百分之一秒
#define DHCPv6OPT_UNICAST       12 //* Server Unicast Option，客户端一旦收到该选项，则这之后的报文均通过选项携带的地址与服务器通讯
#define DHCPv6OPT_SCODE         13 //* Status Code Option，与IAA等需要指示操作结果的选项关联的选项，其既可以作为选项直接包含在服务器消息体中也可以作为子选项包含在DHCP的选项中，参见ST_DHCPv6OPT_IAA_HDR及ST_DHCPv6OPT_SCODE_HDR定义
#define DHCPv6OPT_RCOMMIT       14 //* Rapid Commit Option，用于快速交换信息，客户端在发送Solicit请求消息时携带这个选项，服务器携带配置信息和地址租约以及Rapid commit选项以快速响应请求信息，客户端收到该应答后立即生效配置信息，注意这里的重点是回馈信息也携带Rapi Commit Option，如此才可以生效配置信息
#define DHCPv6OPT_RECONF        19 //* Reconfigure Message Option，由Reconfigure（10）消息携带此选项，服务器用此选项通知客户端进行重新配置，其通过该选项携带的消息类型(msg-type)字段告诉客户端采用哪种消息类型（Renew 5、Rebind 6、Info-req 11）进行重新配置
#define DHCPv6OPT_RECONF_ACCEPT 20 //* Reconfigure Accept Option，客户端通过在报文中携带此选项告诉服务器自己同意接受Reconfigure（10）消息，服务器携带此选项则是询问客户端是否同意接受Reconfigure（10）消息，此选项的接收方如果同意则携带此选项回馈对方即可，否则忽略，缺省是客户端不同意
#define DHCPv6OPT_RDNSSRV       23 //* DNS Recursive Name Server option, 关于这个选项，参见[RFC3646]：https://www.rfc-editor.org/rfc/rfc3646 

//* DHCPv6帧头部结构体
PACKED_BEGIN
typedef union _UNI_DHCPv6_HDR_ {
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

//* DHCPv6选项 - Client/Server Identifier Option，根据[RFC8415]11.1节描述，客户端/服务器ID（DUID，DHCP Unique Identifier）共四种类型，详见：https://www.rfc-editor.org/rfc/rfc8415.html#section-11.1 
//* DUID类型定义如下：
#define DUID_LLT  1 //* DUID Based on Link-Layer Address Plus Time
#define DUID_EN   2 //* DUID Assigned by Vendor Based on Enterprise Number
#define DUID_LL   3 //* DUID Based on Link-Layer Address (DUID-LL)
#define DUID_UUID 4 //* DUID Based on Universally Unique Identifier

//* 硬件类型（Hardware Type）定义，详细、完整的类型定义参见：https://www.iana.org/assignments/arp-parameters/arp-parameters.xhtml#arp-parameters-2
//* 这里仅定义协议栈现在以及将来计划支持的硬件类型
#define DHW_ETHERNET 1 //* Ethernet
#define DHW_IEEE802  6 //* IEEE 802 Networks

//* DUID_LLT结构定义
PACKED_BEGIN
typedef struct _ST_DUID_LLT_ETH_ { 
	USHORT usType;         //* 指定DUID类型	
	USHORT usHardwareType; //* 硬件类型
	UINT unSecs; //*  unix时间戳，自从2000年1月1日零点零分零秒之后开始的秒数，也就是当前unix秒数减去946656000即可
	UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN]; 
} PACKED ST_DUID_LLT_ETH, *PST_DUID_LLT_ETH;
PACKED_END

//* DUID_EN结构定义
PACKED_BEGIN
typedef struct _ST_DUID_EN_HDR_ {
	USHORT usType;   //* 指定DUID类型	
	UINT unEnterNum; //* 企业编号
	/* */ //* 可变长度的企业标识
} PACKED ST_DUID_EN_HDR, *PST_DUID_EN_HDR;
PACKED_END

//* DUID_LL结构定义
PACKED_BEGIN
typedef struct _ST_DUID_LL_ETH_ {
	USHORT usType;         //* 指定DUID类型	
	USHORT usHardwareType; //* 硬件类型	
	UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN];
} PACKED ST_DUID_LL_ETH, *PST_DUID_LL_ETH;
PACKED_END

//* DUID_UUID结构定义
PACKED_BEGIN
typedef struct _ST_DUID_UUID_ {
	USHORT usType; //* 指定DUID类型	
	UCHAR ubaUUID[16]; 
} PACKED ST_DUID_UUID, *PST_DUID_UUID;
PACKED_END

PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_DUID_HDR_ {
	ST_DHCPv6OPT_HDR stHdr;	
	/* …… */ //* 可变长度的标识
} PACKED ST_DHCPv6OPT_DUID_HDR, *PST_DHCPv6OPT_DUID_HDR;
PACKED_END

//* DHCPv6选项 - Identity Association for Non-temporary Addresses Option
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

//* DHCPv6选项 - Identity Association for Temporary Addresses Option
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_IATA_HDR_ {
	ST_DHCPv6OPT_HDR stHdr;
	UINT unId; //* IA标识				   
	/* …… */ //* IANA选项，变长，通过选项IAA携带的临时Ipv6地址，同样其可能携带多个IAA选项（多个地址）
} PACKED ST_DHCPv6OPT_IATA_HDR, *PST_DHCPv6OPT_IATA_HDR;
PACKED_END

//* DHCPv6选项 - IA Address Option，IANA与IATA的子选项，携带出租/租用的地址或请求/分配的临时地址
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_IAA_HDR_ {
	ST_DHCPv6OPT_HDR stHdr;
	UCHAR ubaIpv6Addr[16];    //* 出租/租用的地址或请求/分配的临时地址，根据[RFC8415]24.6节关于"IPv6-address"字段的描述，这个地址其前缀128位长
	UINT unPreferredLifetime; //* 推荐给节点选用的生存时间，单位：秒
	UINT unValidLifetime;     //* 有效生存时间，单位：秒，全1表示无限长，否则到期则地址失效，将不再使用，应谨慎使用
	/* …… */ //* 可能会存在IAaddr-options字段，如果该IA地址存在操作错误，则IAA选项会携带SCODE子选项指示当前操作结果
} PACKED ST_DHCPv6OPT_IAA_HDR, *PST_DHCPv6OPT_IAA_HDR;
PACKED_END

//* DHCPv6选项 - Preference Option，优先级选项
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_PRF_HDR_ {
	ST_DHCPv6OPT_HDR stHdr;
	UCHAR ubPrf; //* 优先级，最大255，用于服务器选择，大者胜出
} PACKED ST_DHCPv6OPT_PRF_HDR, *PST_DHCPv6OPT_PRF_HDR;
PACKED_END

//* DHCPv6选项 - Elapsed Time option，用于计量完成整个交互完成的时间，单位：百分之一秒。客户端发送的第一个Solicit报文该值为0
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_ETIME_ {
	ST_DHCPv6OPT_HDR stHdr;
	USHORT usElapsedTime; //* 从第一个报文开始到结束整个请求耗时多少个百分之一秒
} PACKED ST_DHCPv6OPT_ETIME, *PST_DHCPv6OPT_ETIME;
PACKED_END

//* DHCPv6选项 - Server Unicast Option
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_UNICAST_ {
	ST_DHCPv6OPT_HDR stHdr;
	UCHAR ubaSrvAddr[16]; //* 客户端报文的目的地址，收到该选项后不再发往组播地址
} PACKED ST_DHCPv6OPT_UNICAST, *PST_DHCPv6OPT_UNICAST; 
PACKED_END

//* Status Code定义
#define Dv6SCODE_SUCCESS      0      //* Success.
#define Dv6SCODE_UNSPECFAIL   1      //* Failure, reason unspecified. 
#define Dv6SCODE_NOADDRS      2      //* The server has no addresses available to assign to the IA(s). 
#define Dv6SCODE_NOBINDING    3      //* Client record (binding) unavailable. 
#define Dv6SCODE_NOTONLINK    4      //* The prefix for the address is not appropriate for the link to which the client is attached. 
#define DV6SCODE_USEMULTICAST 5      //* Sent by a server to a client to force the client to send messages to the server using the All_DHCP_Relay_Agents_and_Servers multicast address. 
#define Dv6SCODE_NOPREFIX     6      //* The server has no prefixes available to assign to the IA_PD(s). 
#define Dv6SCODE_UNASSIGNED   65535  //* Unassigned

//* DHCPv6选项 - Status Code Option
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_SCODE_HDR_ {
	ST_DHCPv6OPT_HDR stHdr;
	USHORT usCode; //* 状态码，详细完整且处于持续更新状态的状态码定义参见：https://www.iana.org/assignments/dhcpv6-parameters/dhcpv6-parameters.xhtml#dhcpv6-parameters-5
	               //* 协议栈用到的状态码参见[RFC8415]21.13节“Table 3: Status Code Definitions”：https://www.rfc-editor.org/rfc/rfc8415.html#section-21.13

	/* …… */ //* 可变长度的utf8编码的给终端用户显式的状态消息（status-message）
} PACKED ST_DHCPv6OPT_SCODE_HDR, *PST_DHCPv6OPT_SCODE_HDR;
PACKED_END

//* DHCPv6选项 - Reconfigure Message Option，由服务器下发的DHCPv6MSGTYPE_RECONFIGURE消息携带，通知客户端此时需要重新配置续租或更新配置参数
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_RECONF_ {
	ST_DHCPv6OPT_HDR stHdr;
	UCHAR ubMsgType; //* 消息类型：DHCPv6MSGTYPE_RENEW、DHCPv6MSGTYPE_REBIND、DHCPv6MSGTYPE_INFOREQUEST
} PACKED ST_DHCPv6OPT_RECONF, *PST_DHCPv6OPT_RECONF; 
PACKED_END

//* DHCPv6选项 - DNS Recursive Name Server option，详见：https://www.rfc-editor.org/rfc/rfc3646#section-3
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_RDNSSRV_HDR_ {
	ST_DHCPv6OPT_HDR stHdr; 
	UCHAR ubaIpv6[16]; //* 第1个递归DNS服务器地址
	/* …… */ //* 第x个递归DNS服务器地址
} PACKED ST_DHCPv6OPT_RDNSSRV_HDR, *PST_DHCPv6OPT_RDNSSRV_HDR;
PACKED_END

//* DHCPv6选项 - Option Request Option，用于Solicit报文的ORO结构体定义
#define OROSOL_OPTNUM 1 //* Solicit选项请求的数量
PACKED_BEGIN
typedef struct _ST_DHCPv6OPT_OROSOL_ {
	ST_DHCPv6OPT_HDR stHdr;
	USHORT usaOptions[OROSOL_OPTNUM]; 
} PACKED ST_DHCPv6OPT_OROSOL, *PST_DHCPv6OPT_OROSOL;
PACKED_END

#define OROREQ_OPTNUM OROSOL_OPTNUM //* Request选项请求的数量
typedef ST_DHCPv6OPT_OROSOL ST_DHCPv6OPT_OROREQ, *PST_DHCPv6OPT_OROREQ; 

#endif
#endif

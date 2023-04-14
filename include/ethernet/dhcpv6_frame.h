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

#if SUPPORT_IPV6

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


#endif
#endif

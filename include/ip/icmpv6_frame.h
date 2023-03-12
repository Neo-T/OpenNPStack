/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * ICMPv6帧结构定义
 *
 * Neo-T, 创建于2023.03.12 09:40
 *
 */
#ifndef ICMPv6_FRAME_H
#define ICMPv6_FRAME_H

typedef enum {
    ICMPv6_ERRDST	= 1,	//* Destination Unreachable，目标不可达
    ICMPv6_ERRPTB	= 2,	//* Packet Too Big，包过大
    ICMPv6_ERRTE	= 3,	//* Time Exceeded，超时
    ICMPv6_ERRPP    = 4,    //* Parameter Problem，参数问题
    ICMPv6_ECHOREQ	= 128,	//* Echo Request,回显请求 
	ICMPv6_ECHOREP	= 129,	//* Echo Reply，回显应答
	ICMPv6_MCLQ		= 130,	//* Multicast Listener Query, 组播监听查询
	ICMPv6_MCLR		= 131,	//* Multicast Listener Report，组播监听报告
	ICMPv6_MCLD		= 132,	//* Multicast Listener Done，组播监听结束
	ICMPv6_RS		= 133,	//* Router Solicitation，路由器请求
    ICMPv6_RA		= 134,	//* Router Advertisement，路由器通告
	ICMPv6_NS		= 135,	//* Neighbor Solicitation，邻居请求
	ICMPv6_NA		= 136,	//* Neighbor Advertisement，邻居通告
	ICMPv6_RM		= 137,	//* Redirect Message，重定向
	ICMPv6_RR		= 138,	//* Router Renumbering，路由器重编号
	ICMPv6_NI		= 139,	//* ICMP Node Information，节点信息查询
	ICMPv6_NR		= 140,	//* ICMP Node Response，节点信息应答
	ICMPv6_INDS		= 141,	//* Inverse Neighbor Discovery Solicitation，反向邻居探索请求
	ICMPv6_INDA		= 142,	//* Inverse Neighbor Discovery Advertisement，反向邻居探索通告
    ICMP_MAX		= 255
} EN_ICMPv6TYPE;

//* ICMPv6_ERRDST，目标不可达错误报文携带的具体错误值定义
typedef enum {
    ERRDST_NOROUTE			= 0, //* No route to destination，无到达指定目的地的路由选项
	ERRDST_ADMINPROHIBITED	= 1, //* Communication with destination administratively prohibited，与目标地址的同学那被管理策略禁止，通常原因是数据包被防火墙丢弃时才会报这个错误
    ERRDST_BSSA				= 2, //* Beyond scope of source address， 超出了源地址作用域，当数据包必须由某个网路接口发送，但这个接口并不在源地址的作用范围内时，路由器发送该错误信息
	ERRDST_AU				= 3, //* Address unreachable， 地址不可达，当路由器无法解析目标链路层地址时发送该错误
    ERRDST_PU				= 4, //* Port unreachable， 端口不可达，目标设备未开启目的端口时发送该错误
    ERRDST_SAFIEP			= 5, //* Source address failed ingress/egress policy，源地址未通过出入站策略的检查
    ERRDST_RR				= 6, //* Reject route to destination，当数据包匹配某条拒绝路由条目时发送这个错误。其实就是路由器上配置的地址前缀黑名单，数据包目的地址前缀在黑名单中的都将被丢弃，同时发送该错误
} EN_ERRDST;

//* ICMPv6_ERRTE，超时错误报文携带的具体错误值定义
typedef enum {
    ERRTE_HLE = 0, //* Hop limit exceeded in transit，超出跳数限制，发送或到达的报文其IPv6头部中的跳数限制字段值减为0
	ERRTE_FR  = 1, //* Fragment reassembly time exceeded，目的主机分片重组超时
} EN_ERRSRC;

//* ICMPv6_ERRPP，参数问题
typedef enum {
	ERRPP_EHF  = 0, //* Erroneous header field encountered，遇到IPv6头部或扩展头部中的某字段错误
    ERRPP_UNHT = 1, //* Unrecognized Next Header type encountered，Next Header字段的值无法识别
    ERRPP_UOPT = 2, //* Unrecognized IPv6 option encountered，无法识别的IPv6可选项
} EN_ERRREDIRECT;

 //* Icmpv6帧头部结构体
PACKED_BEGIN
typedef struct _ST_ICMPv6_HDR_ {
    UCHAR ubType; 
	UCHAR ubCode; 
	USHORT usChecksum; 	
} PACKED ST_ICMPv6_HDR, *PST_ICMPv6_HDR; 
PACKED_END

//* Neighbor Solicitation，邻居请求消息头部结构体
PACKED_BEGIN
typedef struct _ST_ICMPv6_NS_HDR_ {
    UINT unReserved;		 //* 保留字段
	UCHAR ubaTargetAddr[16]; //* 目标地址
} PACKED ST_ICMPv6_NS_HDR, *PST_ICMPv6_NS_HDR;
PACKED_END

//* S/TLLA，Source/Target link-layer address选项
PACKED_BEGIN
typedef struct _ST_ICMPv6_OPT_LLA_ {
	UCHAR ubType;		//* 选项类型
	UCHAR ubLen;		//* 长度，含ubType、ubLen字段
	UCHAR ubaAddr[6];	//* 源/目标链路层地址（对于ethernet则是mac地址）
} PACKED ST_ICMPv6_OPT_LLA, *PST_ICMPv6_OPT_LLA;
PACKED_END

//* Neighbor Advertisement，邻居通告消息头部结构体
PACKED_BEGIN
typedef struct _ST_ICMPv6_NA_HDR_ {
	union {
		struct {
			UINT bitReserved : 29;	//* 保留
			UINT bitOverride : 1;	//* 覆盖标记，置1，表示报文携带的TLLA选项中的链路层地址应该覆盖IPv6 To Link layer addr映射表中已缓存的条目；置0，则表示不覆盖，除非缓存条目中不存在该链路层地址映射，此时需要新增条目
			UINT bitSolicited : 1;	//* 请求标记，置1表示这是对NS消息的响应，对于组播邻居节点通告及未发送过NS消息的单播NA，该位置0，该位还可以用来执行邻居不可达性检测确认
			UINT bitRouter : 1;		//* 路由器标记，标记当前NA消息发送方是否位路由器，置1位路由器，置0则否，在邻居不可达检测中检测路由器是否变成主机
		} stb32; 
		UINT unVal; 
	} uniFlag;
	UCHAR ubaTargetAddr[16]; //* 目标地址，对NS消息响应时，此地址应为NS消息中携带的目标地址字段值，非NS消息响应时，此应为链路层地址发生变换的IPv6地址。该字段说白了就是NA要通告的IPv6地址，其携带的目标链路层地址可选项值与之对应
} PACKED ST_ICMPv6_NA_HDR, *PST_ICMPv6_NA_HDR;
PACKED_END

#endif

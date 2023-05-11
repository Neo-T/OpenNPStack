/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * ICMPv6帧结构定义，结构体各字段来源特别是消息类型及选项类型参见：
 * https://www.iana.org/assignments/icmpv6-parameters/icmpv6-parameters.xhtml
 * 
 * Neo-T, 创建于2023.03.12 09:40
 *
 */
#ifndef ICMPv6_FRAME_H
#define ICMPv6_FRAME_H

#if SUPPORT_IPV6
#define IPv6MCTOMACADDR_PREFIX 0x33	//* ipv6版本的以太网mac组播地址前缀

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
	ICMPv6_RR		= 137,	//* Router Redirect Message，重定向
	ICMPv6_RN		= 138,	//* Router Renumbering，路由器重编号
	ICMPv6_NI		= 139,	//* ICMP Node Information，节点信息查询
	ICMPv6_NR		= 140,	//* ICMP Node Response，节点信息应答
	ICMPv6_INDS		= 141,	//* Inverse Neighbor Discovery Solicitation，反向邻居探索请求
	ICMPv6_INDA		= 142,	//* Inverse Neighbor Discovery Advertisement，反向邻居探索通告
	ICMPv6_MLRMv2	= 143,	//* Multicast Listener Report Message v2，组播侦听报告消息版本2，该类型用于支持MLDv2的侦听节点即时声明自己对哪些特定组播地址感兴趣
    ICMPv6_MAX		= 255
} EN_ICMPv6TYPE;

//* ICMPv6_ERRDST，目标不可达错误报文携带的具体错误值定义
typedef enum {
    ERRDST_NOROUTE			= 0, //* No route to destination，无到达指定目的地的路由选项
	ERRDST_ADMINPROHIBITED	= 1, //* Communication with destination administratively prohibited，与目标地址的通讯被管理策略禁止，通常原因是数据包被防火墙丢弃时才会报这个错误
    ERRDST_BSSA				= 2, //* Beyond scope of source address， 超出了源地址作用域，当数据包必须由某个网路接口发送，但这个接口并不在源地址的作用范围内时，路由器发送该错误信息
	ERRDST_AU				= 3, //* Address unreachable， 地址不可达，当路由器无法解析目标链路层地址时发送该错误
    ERRDST_PU				= 4, //* Port unreachable， 端口不可达，目标设备未开启目的端口时发送该错误
    ERRDST_SAFIEP			= 5, //* Source address failed ingress/egress policy，源地址未通过出入站策略的检查
    ERRDST_RR				= 6, //* Reject route to destination，当数据包匹配某条拒绝路由条目时发送这个错误。其实就是路由器上配置的地址前缀黑名单，数据包目的地址前缀在黑名单中的都将被丢弃，同时发送该错误
} EN_ICMPv6ERRDST;

//* ICMPv6_ERRTE，超时错误报文携带的具体错误值定义
typedef enum {
    ERRTE_HLE = 0, //* Hop limit exceeded in transit，超出跳数限制，发送或到达的报文其IPv6头部中的跳数限制字段值减为0
	ERRTE_FR  = 1, //* Fragment reassembly time exceeded，目的主机分片重组超时
} EN_ICMPv6ERRSRC;

//* ICMPv6_ERRPP，参数问题
typedef enum {
	ERRPP_EHF  = 0, //* Erroneous header field encountered，遇到IPv6头部或扩展头部中的某字段错误
    ERRPP_UNHT = 1, //* Unrecognized Next Header type encountered，Next Header字段的值无法识别
    ERRPP_UOPT = 2, //* Unrecognized IPv6 option encountered，无法识别的IPv6可选项
} EN_ICMPv6ERRREDIRECT;

//* icmpv6消息报文携带option类型值定义
#define ICMPV6OPT_SLLA	1	//* Source link-layer address，源链路层地址可选项
#define ICMPV6OPT_TLLA	2	//* Target link-layer address，目标链路层地址可选项

//* Icmpv6帧头部结构体
PACKED_BEGIN
typedef struct _ST_ICMPv6_HDR_ {
    UCHAR ubType; 
	UCHAR ubCode; 
	USHORT usChecksum; 	
} PACKED ST_ICMPv6_HDR, *PST_ICMPv6_HDR; 
PACKED_END

//* Echo Request，回显头部结构体定义
PACKED_BEGIN
typedef struct _ST_ICMPv6_ECHO_HDR_ {
	USHORT usIdentifier;
	USHORT usSeqNum;
} PACKED ST_ICMPv6_ECHO_HDR, *PST_ICMPv6_ECHO_HDR;
PACKED_END

//* Ipv6邻居发现协议Neighbor Discovery for IP version 6 (IPv6)详见：https://www.rfc-editor.org/rfc/rfc4861.html
//* Neighbor Solicitation，邻居请求消息头部结构体
PACKED_BEGIN
typedef struct _ST_ICMPv6_NS_HDR_ {
    UINT unReserved;		 //* 保留字段
	UCHAR ubaTargetAddr[16]; //* 目标地址
} PACKED ST_ICMPv6_NS_HDR, *PST_ICMPv6_NS_HDR;
PACKED_END

//* Router Solicitation，邻居请求消息头部结构体
PACKED_BEGIN
typedef struct _ST_ICMPv6_RS_HDR_ {
	UINT unReserved;		 //* 保留字段
} PACKED ST_ICMPv6_RS_HDR, *PST_ICMPv6_RS_HDR;
PACKED_END

//* Neighbor Advertisement，邻居通告消息头部结构体
PACKED_BEGIN
typedef struct _ST_ICMPv6_NA_HDR_ {
	union {
		struct {
			UINT bitReserved  : 29; //* 保留
			UINT bitOverride  : 1;  //* 覆盖标记，置1，表示报文携带的TLLA选项中的链路层地址应该覆盖IPv6 To Link layer addr映射表中已缓存的条目；置0，则表示不覆盖，除非缓存条目中不存在该链路层地址映射，此时需要新增条目
			UINT bitSolicited : 1;  //* 请求标记，置1表示这是对NS消息的响应，对于组播邻居节点通告及未发送过NS消息的单播NA，该位置0，该位还可以用来执行邻居不可达性检测确认
			UINT bitRouter    : 1;  //* 路由器标记，标记当前NA消息发送方是否位路由器，置1位路由器，置0则否，在邻居不可达检测中检测路由器是否变成主机
		} PACKED stb32;
		UINT unVal; 
	} PACKED uniFlag;
	UCHAR ubaTargetAddr[16]; //* 目标地址，对NS消息响应时，此地址应为NS消息中携带的目标地址字段值，非NS消息响应时，此应为链路层地址发生变换的IPv6地址。该字段说白了就是NA要通告的IPv6地址，其携带的目标链路层地址可选项值与之对应
} PACKED ST_ICMPv6_NA_HDR, *PST_ICMPv6_NA_HDR;
PACKED_END
#define icmpv6_na_flag_o uniFlag.stb32.bitOverride
#define icmpv6_na_flag_s uniFlag.stb32.bitSolicited
#define icmpv6_na_flag_r uniFlag.stb32.bitRouter
#define icmpv6_na_flag   uniFlag.unVal

//* Router Advertisement，路由器通告（RA）消息头部结构体，详见[RFC4861] 4.2节：https://www.rfc-editor.org/rfc/rfc4861#section-4.2
PACKED_BEGIN
typedef struct _ST_ICMPv6_RA_HDR_ {
	UCHAR ubHopLimit; 
	union 
	{
		struct { //* icmpv6_ra_flag_m和icmpv6_ra_flag_o标记的详细说明参见[RFC4861]4.2节：https://www.rfc-editor.org/rfc/rfc4861#section-4.2
			UCHAR bitReserved : 2;
			UCHAR bitProxy    : 1;
			UCHAR bitPrf      : 2; //* 默认路由器优先级，01：高；00：中；11：低；10：为保留值，如果收到则将其视为00值处理，详见[RFC4191] 2.2节：https://www.rfc-editor.org/rfc/rfc4191.html
			UCHAR bitAgent    : 1; //* RFC 3775为移动ipv6准备
			UCHAR bitOther    : 1; //* Other Configuration，O标志，当M标志为0时该位才会被启用，也就是此时程序才会去关注这个标志。当其置位，且icmpv6 option - Prefix information中A标志置位则协议栈将通过DHCPv6获得其它参数，否则不通过DHCPv6获得其它参数
			UCHAR bitManaged  : 1; //* Managed address configuration，M标志，指示是否配置有状态ipv6地址。置位：无状态配置结束后可以通过DHCPv6进行地址配置（获得的ipv6地址及dns等）；反之则不支持通过DHCPv6进行地址配置
		} PACKED stb8;
		UCHAR ubVal; 
	} PACKED uniFlag;
	USHORT usLifetime;    //* 路由器生存时间，如果为0则其不能作为默认路由器，也就是默认网关，同时按照[RFC4191]2.2节的规定bitPref位也应为00，bitPrf值将被接收者忽略，该路由器将从缺省路由器列表中被删除，详见[RFC2461]4.2节：https://www.rfc-editor.org/rfc/rfc2461#section-4.2
	UINT unReachableTime; //* 节点可达时间，为0表示路由器没有指定可达时间
	UINT unRetransTimer;  //* 重发RS报文的间隔时间，为0表示路由器没有指定
} PACKED ST_ICMPv6_RA_HDR, *PST_ICMPv6_RA_HDR;
PACKED_END
#define icmpv6_ra_flag_prf uniFlag.stb8.bitPrf
#define icmpv6_ra_flag_a   uniFlag.stb8.bitAgent
#define icmpv6_ra_flag_o   uniFlag.stb8.bitOther
#define icmpv6_ra_flag_m   uniFlag.stb8.bitManaged
#define icmpv6_ra_flag     uniFlag.ubVal

//* 邻居发现（Neighbor Discovery for IP version 6）相关选项类型，详细的类型定义参见（IPv6 Neighbor Discovery Option Formats）：
//* https://www.iana.org/assignments/icmpv6-parameters/icmpv6-parameters.xhtml#icmpv6-parameters-5
#define ICMPv6NDOPT_SRCLNKADDR	1  //* Source Link-layer Address
#define ICMPv6NDOPT_TRGLNKADDR	2  //* Target Link-layer Address
#define ICMPv6NDOPT_PREFIXINFO	3  //* Prefix Information
#define ICMPv6NDOPT_REDIRECTHDR	4  //* Redirected Header
#define ICMPv6NDOPT_MTU			5  //* mtu，确保链路上的所有节点采用相同的mtu
#define ICMPv6NDOPT_ROUTERINFO	24 //* Route Information，参见[RFC4191] 2.3节，其存在的目的就是代替传统的Prefix Information Option
#define ICMPv6NDOPT_RDNSSRV		25 //* Recursive DNS Server Option，其由[RFC8106]/[RFC5006] "IPv6 Router Advertisement Option for DNS Configuration"定义，专门用于dns配置
#define ICMPv6NDOPT_DNSSCHLIST	31 //* DNS Search List Option，参见[RFC8106]

//* 邻居发现协议携带的选项头
PACKED_BEGIN
typedef struct _ST_ICMPv6NDOPT_HDR_ {
	UCHAR ubType; //* 选项类型
	UCHAR ubLen;  //* 长度，含ubType、ubLen字段，单位：8字节	
} PACKED ST_ICMPv6NDOPT_HDR, *PST_ICMPv6NDOPT_HDR;
PACKED_END

//* S/TLLA，Source/Target link-layer address option，SLLA用于NS、RS、RA报文，TLLA用于NA、RR报文
PACKED_BEGIN
typedef struct _ST_ICMPv6NDOPT_LLA_ {
	ST_ICMPv6NDOPT_HDR stHdr; 
	UCHAR ubaAddr[6]; //* 源/目标链路层地址（对于ethernet则是mac地址）
} PACKED ST_ICMPv6NDOPT_LLA, *PST_ICMPv6NDOPT_LLA;
PACKED_END

//* Prefix information option，仅用于RA报文，其它类型报文出现应直接丢弃
PACKED_BEGIN
typedef struct _ST_ICMPv6NDOPT_PREFIXINFO_ {
	ST_ICMPv6NDOPT_HDR stHdr;
	UCHAR ubPrefixBitLen;  //* 前缀位长
	union {
		struct {
			UCHAR bitReserved    : 6; //* 保留字段 
			UCHAR bitAutoAddrCfg : 1; //* 地址配置
			UCHAR bitOnLink      : 1; //* 在链路标志，如果置位，则这个前缀可以用于确定所有属于该前缀的地址（可能分布在不同主机）的on-link（在链路）状态，否则不能确定其
			                          //* 是否on-link还是off-link，比如该前缀用于地址配置时，存在一些前缀地址在链路上，另一些可能正在配置处于链路外
		} PACKED stb8;
		UCHAR ubVal; 
	} PACKED uniFlag; 
	UINT unValidLifetime;     //* 有效生存时间，单位：秒，全1表示无限长，否则到期则地址失效，将不再使用
	UINT unPreferredLifetime; //* 推荐给节点选用的生存时间，单位：秒，全1表示无限长，这个时间小于等于有效生存时间
	UINT unReserved;		  //* 保留，但必须全零
	UCHAR ubaPrefix[16];	  //* 注意，如果路由器发布了链路本地地址前缀（FE80::），直接忽略即可，另外，非前缀数据位必须清零
} PACKED ST_ICMPv6NDOPT_PREFIXINFO, *PST_ICMPv6_NDOPT_PREFIXINFO;
PACKED_END
#define icmpv6ndopt_pi_flag_A uniFlag.stb8.bitAutoAddrCfg
#define icmpv6ndopt_pi_flag_L uniFlag.stb8.bitOnLink
#define icmpv6ndopt_pi_flag uniFlag.ubVal

//* Redirected Header option，仅用于RR报文
PACKED_BEGIN
typedef struct _ST_ICMPv6NDOPT_REDIRECTED_HDR_ {
	ST_ICMPv6NDOPT_HDR stHdr; 
	USHORT usReserved; //* 这之后为IP头加上层协议报文
} ST_ICMPv6NDOPT_REDIRECTED_HDR, *PST_ICMPv6NDOPT_REDIRECTED_HDR;
PACKED_END

//* MTU option，仅用于RA报文，以确保链路上的所有节点使用相同的MTU值
PACKED_BEGIN
typedef struct _ST_ICMPv6NDOPT_MTU_ {
	ST_ICMPv6NDOPT_HDR stHdr; 
	USHORT usReserved; 
	UINT unMtu; //* 通告的路由器mtu值 
} ST_ICMPv6NDOPT_MTU, *PST_ICMPv6NDOPT_MTU;
PACKED_END

//* Route Information option，仅用于RA报文，其被设计用于替代Prefix Infomation选项，详见[RFC4191] 2.2及2.3节：https://www.rfc-editor.org/rfc/rfc4191.html#section-2.2
//* 关于其中的prf字段与RA报文头部prf字段同时存在时如何处理的说明，参见：https://www.rfc-editor.org/rfc/rfc4191.html#section-3
//* 在这里协议栈选择了Type B处理方式，路由器信息选项将被忽略，详见[RFC4191]第三节：Conceptual Model of a Host及第4节：Router Configuration
PACKED_BEGIN
typedef struct _ST_ICMPv6NDOPT_ROUTERINFO_ {
	ST_ICMPv6NDOPT_HDR stHdr;
	UCHAR ubPrefixBitLen;  //* 前缀位长
	union {
		struct {
			UCHAR bitReserved1 : 3; //* 保留字段 
			UCHAR bitPrf       : 2; //* 路由器优先级，如果存在多个拥有相同前缀的路由器时，该字段指定优先选用哪个路由器，参见ST_ICMPv6_RA_HDR::uniFlag::stb8::bitPrf字段值说明
			                        //* 唯一的不同是当收到保留的值10时，该Router information选项将被接收端忽略
			UCHAR bitReserved2 : 3; //* 保留字段 
		} PACKED stb8;
		UCHAR ubVal;
	} PACKED uniFlag;
	UINT unLifetime; //* 路由器生存时间，0xFFFFFFFF为无限长
	/* …… *///* 可变长度的前缀数据，其长度为0，8或16字节，这取决于ubPrefixBitLen字段值
} ST_ICMPv6NDOPT_ROUTERINFO, *PST_ICMPv6NDOPT_ROUTERINFO;
PACKED_END

//* Recursive DNS Server Option，仅用于RA报文
PACKED_BEGIN
typedef struct _ST_ICMPv6NDOPT_RDNSSRV_HDR_ {
	ST_ICMPv6NDOPT_HDR stHdr;	
	USHORT usReserved; 
	UINT unLifetime; //* RDNS服务器生存时间，0xFFFFFFFF为无限长
	/* …… *///*递归DNS服务器地址列表，一个或多个Ipv6地址，其数量等于(stHdr.ubLen - 1) / 2
} ST_ICMPv6NDOPT_RDNSSRV_HDR, *PST_ICMPv6NDOPT_RDNSSRV_HDR;
PACKED_END

//* DNS Search List option, DNSSL，DNS搜索列表，限于RA报文，目前仅在这里先定义,协议栈暂不支持
//* 关于DNSSL：http://ipv6hawaii.org/?p=506
//* DNS搜索列表是在键入长FQDN（完全限定域名，Fully Qualified Domain Names）时保存的快捷方式或方法。例如，如果我家里的服务器有DNS名称：nas.example.com、router.example.com
//* 和music.example.com，而没有搜索列表，那么每次我想访问每台服务器时，我都必须键入FQDN。但是使用example.com的搜索列表，DNS客户端会自动将example.com附加到我的每个查询中。
//* 这样，我只需要键入nas，DNS客户端就会查询nas.example.com，与DNS服务的地址一样，DNSSL是通过类似的方法分发的，使用DHCPv4、DHCPv6和RA DNSSL选项（RFC 8106 Sect 5.2）。
//* 不幸的是，目前并不是所有主机都支持RA中的DNSSL。例如，ChromeOS会忽略RA DNSSL选项，并且必须键入FQDN。
PACKED_BEGIN
typedef struct _ST_ICMPv6NDOPT_DNSSL_HDR_ {
	ST_ICMPv6NDOPT_HDR stHdr;
	USHORT usReserved;
	UINT unLifetime; //* RDNS服务器生存时间，0xFFFFFFFF为无限长	
	/* …… *///* 一个或多个域名，每个域名其实就是以0结尾的标签序列，每个标签之间也就是"."之间用一个字节表示标签长度（实际只使用了前6位，剩下的高两位必须为0，所以标签最大长度63字节），0填充不足8字节倍数的部分
} ST_ICMPv6NDOPT_DNSSL_HDR, *PST_ICMPv6NDOPT_DNSSL_HDR; 
PACKED_END


#endif

#endif

/* ppp_frame.h
 *
 * ppp帧结构定义
 *
 * Neo-T, 创建于2022.03.21 14:04
 * 版本: 1.0
 *
 */
#ifndef PPP_FRAME_H
#define PPP_FRAME_H
#include "ppp_protocols.h"

//* LCP、PAP、CHAP、NCP等协议基本结构定义
//* ===============================================================================================
//* PPP帧头部结构体
PACKED_BEGIN
typedef struct _ST_PPP_HDR_ {
	UCHAR ubFlag;		//* 标志域，固定字符（参见PPP_FLAG宏），其界定一个完整的PPP帧
	UCHAR ubAddr;		//* 地址域，固定为PPP_ALLSTATIONS
	UCHAR ubCtl;		//* 控制域，固定为PPP_UI
	USHORT usProtocol;	//* 协议域，PPP帧携带的协议类型，参见ppp_protocols.h中前缀为PPP_的相关宏定义，如PPP_IP为IP协议
} PACKED ST_PPP_HDR, *PST_PPP_HDR;
PACKED_END

//* PPP帧尾部结构体
PACKED_BEGIN
typedef struct _ST_PPP_TAIL_ {
	USHORT usFCS;		//* 校验和
	UCHAR ubDelimiter;	//* 定界符，与头部标志字符完全相同（参见PPP_FLAG宏）
} PACKED ST_PPP_TAIL, *PST_PPP_TAIL;
PACKED_END

PACKED_BEGIN
typedef struct _ST_LNCP_HDR_ { //* LCP/NCP帧头部结构体
	UCHAR ubCode;		//* 代码域，标识LCP帧报文类型
	UCHAR ubIdentifier;	//* 标识域，唯一的标识一个报文，用于确定应答报文
	USHORT usLen;		//* 长度域：代码域 + 标识域 + 长度域 + 可边长的数据域
} PACKED ST_LNCP_HDR, *PST_LNCP_HDR;
PACKED_END

PACKED_BEGIN
typedef struct _ST_LNCP_ACFC_HDR_ { //* LCP/NCP帧头部结构体，PPP帧地址、控制域压缩时的头部结构
	UCHAR ubFlag;		//* 标志域，固定字符（参见PPP_FLAG宏），其界定一个完整的PPP帧
	USHORT usProtocol; 	//* 协议域，PPP帧携带的协议类型，参见ppp_defs.h中前缀为PPP_的相关宏定义，如PPP_IP为IP协议
	UCHAR ubCode;		//* 代码域，标识LCP帧报文类型
	UCHAR ubIdentifier;	//* 标识域，唯一的标识一个报文，用于确定应答报文
	USHORT usLen;		//* 长度域：代码域 + 标识域 + 长度域 + 可边长的数据域
} PACKED ST_LNCP_ACFC_HDR, *PST_LNCP_ACFC_HDR;
PACKED_END

//* 相关控制协议（LCP、IPCP等）的代码域类型定义
#define CPCODE_NUM	11	//* 协商阶段代码域类型数量
typedef enum {
	CONFREQ = 1,	//* Configure Request，配置请求
	CONFACK = 2,	//* Configure Ack，配置请求通过应答
	CONFNAK = 3,	//* Configure Nak，配置请求的未完全通过应答
	CONFREJ = 4,	//* Configure Reject，配置请求被拒绝（意味着所有配置项均未通过，换言之接收端完全不支持发送端请求的所有配置项）
	TERMREQ = 5,	//* Terminate Request，链路结束请求
	TERMACK = 6,	//* Terminate Ack，链路结束请求应答
	CODEREJ = 7,	//* Code Reject，非法类型，拒绝执行，接收端会在应答报文中将所拒绝报文的全部内容附上

	PROTREJ = 8,	//* Protocol Reject，当PPP帧头中协议域类型非法时，接收端回应该类型的报文给发送端（数据域携带协议类型和数据）
					//* 注意该报文只有在LCP状态机处于Opened状态时才会被发送，其它状态接收端直接丢弃协议类型错误的报文

	ECHOREQ = 9,	//* Echo Request，用以提供数据链路层的环回机制，该类型报文只有在LCP状态机处于Opened状态时才被允许处理，其它状态会被接收端直接丢弃
	ECHOREP = 10,	//* Echo Reply，同上，Request的应答报文
	DISCREQ = 11,	//* Discard Request，提供了一种在数据链路层上的测试机制，一方发送该类型报文，另一方接收后直接丢弃
} EN_CPCODE;

//* LCP配置请求（CONF_REQ, Configure Request）报文的类型定义
#define LCP_CONFREQ_NUM	7	//* 本程序支持的LCP报文配置请求项的数量
typedef enum {
	MRU = 1,			//* Maximum Receive Unit
	ASYNCMAP = 2,		//* Async Control Character Map
	AUTHTYPE = 3,		//* Authentication Type
	QUALITY = 4,		//* Quality Protocol，链路质量监测协议
	MAGICNUMBER = 5,	//* Magic Number，魔术字

	PCOMPRESSION = 7,	//* Protocol Field Compression，该选项提供了一种压缩数据链路层（即PPP帧）协议域的方法，PPP帧的协议域占两个字节，协商通过后，编号
						//* 小于256的协议压缩为单字节传输，比如PPP_IP，但大于256的则无法压缩。因此，协议栈就就需要同时处理PPP帧协议域占两个字节或1个字节的情形

	ACCOMPRESSION = 8,	//* Address/Control Field Compression，压缩数据链路层（即PPP帧）地址域和控制域，因为这两个为固定值可以不发送，所以PPP帧不带FF即认为对端进行了压缩
} EN_LCP_CONFREQ_TYPE;

//* LCP/NCP配置请求报文单条配置项的头部结构
PACKED_BEGIN
typedef struct _ST_LNCP_CONFREQ_HDR_ {
	UCHAR ubType;	//* 类型域
	UCHAR ubLen;	//* 长度域：类型域 + 长度域 + 可变长数据域
} PACKED ST_LNCP_CONFREQ_HDR, *PST_LNCP_CONFREQ_HDR;
PACKED_END

//* LCP配置请求项之MRU，通知对端“我”这边可以接收多大的包，默认值时1500字节
PACKED_BEGIN
typedef struct _ST_LCP_CONFREQ_MRU_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	USHORT usMRU;	//* 最大接收单元，两个字节
} PACKED ST_LCP_CONFREQ_MRU, *PST_LCP_CONFREQ_MRU;
PACKED_END

//* ASYNCMAP，即ACCM异步链路控制字符映射
PACKED_BEGIN
typedef struct _ST_LCP_CONFREQ_ASYNCMAP_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	UINT unMap;
} PACKED ST_LCP_CONFREQ_ASYNCMAP, *PST_LCP_CONFREQ_ASYNCMAP;
PACKED_END

//* AUTHTYPE，认证类型，该配置项紧跟协议类型值之后为可变长数据域，指定具体协议携带的附加数据
PACKED_BEGIN
typedef struct _ST_LCP_CONFREQ_AUTHTYPE_HDR_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	USHORT usType; //* 协议值：PPP_PAP/PPP_CHAP/其它
} PACKED ST_LCP_CONFREQ_AUTHTYPE_HDR, *PST_LCP_CONFREQ_AUTHTYPE_HDR;
PACKED_END

//* QUALITY，链路质量监测协议，该配置项紧跟协议类型值之后为可变长数据域，指定具体协议携带的附加数据
PACKED_BEGIN
typedef struct _ST_LCP_CONFREQ_QUALITY_HDR_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	USHORT usType; //* 协议值：PPP_LQR/其它
} PACKED ST_LCP_CONFREQ_QUALITY_HDR, *PST_LCP_CONFREQ_QUALITY_HDR;
PACKED_END

//* MAGICNUMBER，魔术字，监测网络是否存在自环情形，即“短路”——自己发给自己
PACKED_BEGIN
typedef struct _ST_LCP_CONFREQ_MAGICNUMBER_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	UINT unNum;
} PACKED ST_LCP_CONFREQ_MAGICNUMBER, *PST_LCP_CONFREQ_MAGICNUMBER;
PACKED_END

//* PCOMPRESSION
PACKED_BEGIN
typedef struct _ST_LCP_CONFREQ_PCOMPRESSION_ {
	ST_LNCP_CONFREQ_HDR stHdr;
} PACKED ST_LCP_CONFREQ_PCOMPRESSION, *PST_LCP_CONFREQ_PCOMPRESSION;
PACKED_END

//* ACCOMPRESSION
PACKED_BEGIN
typedef struct _ST_LCP_CONFREQ_ACCOMPRESSION_ {
	ST_LNCP_CONFREQ_HDR stHdr;
} PACKED ST_LCP_CONFREQ_ACCOMPRESSION, *PST_LCP_CONFREQ_ACCOMPRESSION;
PACKED_END

//* CHAP协议的数据域结构
#define CHAP_CHALLENGE_LEN	16 //* CHAP协议固定使用MD5算法，挑战字符串的长度固定16个字节
PACKED_BEGIN
typedef struct _ST_CHAP_DATA_ {
	UCHAR ubChallengeLen;
	UCHAR ubaChallenge[CHAP_CHALLENGE_LEN];
} ST_CHAP_DATA, *PST_CHAP_DATA;
PACKED_END

typedef enum {
	CHALLENGE = 1,
	RESPONSE = 2,
	SUCCEEDED = 3,
	FAILURE = 4
} EN_CHAPCODE;

//* IPCP配置请求项相关宏及数据结构定义
#define IPCP_CONFREQ_NUM	5	//* 本程序支持的IPCP报文配置请求项的数量
typedef enum {
	ADDRS = 1,					//* IP-Addresses，IP地址选项配置，该选项因为具体应用存在问题现已被停用，只有对端强制要求时才会被启用
	COMPRESSION_PROTOCOL = 2,	//* IP-Compression-Protocol，指定要使用的压缩协议，缺省不进行压缩，所以这个程序同样不处理这个配置项
	ADDR = 3,					//* IP-Addr，协商本地使用的IP地址，数据域全为0则要求对端给分配一个地址，此时对端会在CONFNAK帧中携带这个地址
	PRIMARYDNS = 129,			//* Primary DNS Server addr，协商主DNS服务器地址，数据域全为0则要求对端提供一个DNS地址，对端同样在CONFNAK帧中给出这个地址	

	SECONDARYDNS = 131			//* Secondary DNS server addr, 协商次DNS服务器地址，规则与上同
								//* 另，130和132用于协商NetBIOS网络节点的主、次地址，本系统不支持
} EN_IPCP_CONFREQ_TYPE;
PACKED_BEGIN
typedef struct _ST_IPCP_CONFREQ_ADDR_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	UINT unVal; //* 地址
} PACKED ST_IPCP_CONFREQ_ADDR, *PST_IPCP_CONFREQ_ADDR;
PACKED_END
//* ===============================================================================================

#endif
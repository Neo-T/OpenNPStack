#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <asm-generic/ioctls.h>
#include <linux/if_ppp.h>
#include <net/route.h>
#include <stdexcept>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <thread>
#include <vector>
#include <future>
#include <queue>
#include <map>
#include <unordered_map>
#include <algorithm>
#include "datatype.h"
#include "config.h"
#include "OSAdapter.h"
#include "tools.h"
#include "UdpHelper.h"
#include "SingleConsumerLockFreeQueue.h"
#include "LockFreeQueue.h"
#include "TaskManager.h"
#include "OSSemaphore.h"
#include "OSIPCMutex.h"
#include "OSMutex.h"
#include "OSMsgQueue.h"
#include "Timer.h"
#include "PerformanceTimer.h"
#include "ProcWatchDog.h"
#include "TEA.h"
#include "DBAdapterMySQL.h"
#include "OSDynamicLibLoader.h"
#include "Json.h"
#include "nvwa/EnvFactors.h"
#include "nvwa/EnvStatus.h"
#include "SerialPort.h" 
#include "modem.h"
#include "ppp.h"
#include "watchdog.h" 
#include "hdmi.h"
#include "logs.h"

#define DEBUG_LEVEL 1 //* 调试级别为0的话，则不输出原始报文，为1则输出

#define PPP_DEV_PORT	"/dev/ttyUSB1"
#define AUTH_USER		"card"			//* 其实可以是任意名称
#define AUTH_PASSWORD	"any_char"		//* 可以是任意口令

//* LCP、PAP、CHAP、NCP等协议基本结构定义
//* =======================================================================
//#define PPP_FLAG 0x7E	//* PPP帧标志字段，其实就是一个定界符，由其确定帧首和帧尾，其已在ppp_defs.h中定义，这里不再定义

//* PPP帧头部结构体
typedef struct _ST_PPP_HDR_ { 
	UCHAR ubFlag;		//* 标志域，固定字符（参见PPP_FLAG宏），其界定一个完整的PPP帧
	UCHAR ubAddr;		//* 地址域，固定为0xFF
	UCHAR ubCtl;		//* 控制域，固定为0x03
	USHORT usProtocol;	//* 协议域，PPP帧携带的协议类型，参见ppp_defs.h中前缀为PPP_的相关宏定义，如PPP_IP为IP协议
} PACKED ST_PPP_HDR, *PST_PPP_HDR;

//* PPP帧尾部结构体
typedef struct _ST_PPP_TAIL_ {
	USHORT usFCS;		//* 校验和
	UCHAR ubDelimiter;	//* 定界符，与头部标志字符完全相同（参见PPP_FLAG宏）
} PACKED ST_PPP_TAIL, *PST_PPP_TAIL;

typedef struct _ST_LNCP_HDR_ { //* LCP/NCP帧头部结构体
	UCHAR ubCode;		//* 代码域，标识LCP帧报文类型
	UCHAR ubIdentifier;	//* 标识域，唯一的标识一个报文，用于确定应答报文
	USHORT usLen;		//* 长度域：代码域 + 标识域 + 长度域 + 可边长的数据域
} PACKED ST_LNCP_HDR, *PST_LNCP_HDR;

typedef struct _ST_LNCP_ACFC_HDR_ { //* LCP/NCP帧头部结构体，PPP帧标志、地址、控制域压缩时的头部结构；或者，Linux内核PPP模块已经PPP帧这三个域剥离后的头部结构体（Linux下专属）
	USHORT usProtocol;	//* 协议域，PPP帧携带的协议类型，参见ppp_defs.h中前缀为PPP_的相关宏定义，如PPP_IP为IP协议
	UCHAR ubCode;		//* 代码域，标识LCP帧报文类型
	UCHAR ubIdentifier;	//* 标识域，唯一的标识一个报文，用于确定应答报文
	USHORT usLen;		//* 长度域：代码域 + 标识域 + 长度域 + 可边长的数据域
} PACKED ST_LNCP_ACFC_HDR, *PST_LNCP_ACFC_HDR;

//* 相关控制协议（LCP、IPCP等）的代码域类型定义
#define CPCODE_NUM	11	//* 协商阶段代码域类型数量
typedef enum { 
	CONFREQ = 1,		//* Configure Request，配置请求
	CONFACK = 2,		//* Configure Ack，配置请求通过应答
	CONFNAK = 3,		//* Configure Nak，配置请求的未完全通过应答
	CONFREJ = 4,		//* Configure Reject，配置请求被拒绝（意味着所有配置项均未通过，换言之接收端完全不支持发送端请求的所有配置项）
	TERMREQ = 5,		//* Terminate Request，链路结束请求
	TERMACK = 6,		//* Terminate Ack，链路结束请求应答
	CODEREJ = 7,		//* Code Reject，非法类型，拒绝执行，接收端会在应答报文中将所拒绝报文的全部内容附上

	PROTREJ = 8,		//* Protocol Reject，当PPP帧头中协议域类型非法时，接收端回应该类型的报文给发送端（数据域携带协议类型和数据）
						//* 注意该报文只有在LCP状态机处于Opened状态时才会被发送，其它状态接收端直接丢弃协议类型错误的报文

	ECHOREQ = 9,		//* Echo Request，用以提供数据链路层的环回机制，该类型报文只有在LCP状态机处于Opened状态时才被允许处理，其它状态会被接收端直接丢弃
	ECHOREP = 10,		//* Echo Reply，同上，Request的应答报文
	DISCREQ = 11,		//* Discard Request，提供了一种在数据链路层上的测试机制，一方发送该类型报文，另一方接收后直接丢弃
} EN_CPCODE; 

//* LCP配置请求（CONF_REQ, Configure Request）报文的类型定义
#define LCP_CONFREQ_NUM	7	//* 本程序支持的LCP报文配置请求项的数量
typedef enum {
	MRU           = 1,		//* Maximum Receive Unit
	ASYNCMAP      = 2,		//* Async Control Character Map
	AUTHTYPE      = 3,		//* Authentication Type
	QUALITY       = 4,		//* Quality Protocol，链路质量监测协议
	MAGICNUMBER	  = 5,		//* Magic Number，魔术字

	PCOMPRESSION  = 7,		//* Protocol Field Compression，该选项提供了一种压缩数据链路层（即PPP帧）协议域的方法，PPP帧的协议域占两个字节，协商通过后，编号
							//* 小于256的协议压缩为单字节传输，比如PPP_IP，但大于256的则无法压缩。因此，协议栈就就需要同时处理PPP帧协议域占两个字节或1个字节的情形

	ACCOMPRESSION = 8,		//* Address/Control Field Compression，压缩数据链路层（即PPP帧）地址域和控制域，因为这两个为固定值可以不发送，所以PPP帧不带FF即认为对端进行了压缩
} EN_LCP_CONFREQ_TYPE;

//* LCP/NCP配置请求报文单条配置项的头部结构
typedef struct _ST_LNCP_CONFREQ_HDR_ { 
	UCHAR ubType;	//* 类型域
	UCHAR ubLen;	//* 长度域：类型域 + 长度域 + 可变长数据域
} PACKED ST_LNCP_CONFREQ_HDR, *PST_LNCP_CONFREQ_HDR;

//* LCP配置请求项之MRU，通知对端“我”这边可以接收多大的包，默认值时1500字节
typedef struct _ST_LCP_CONFREQ_MRU_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	USHORT usMRU; //* 最大接收单元，两个字节
} PACKED ST_LCP_CONFREQ_MRU, *PST_LCP_CONFREQ_MRU;

//* ASYNCMAP，即ACCM异步链路控制字符映射
typedef struct _ST_LCP_CONFREQ_ASYNCMAP_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	UINT unMap; 
} PACKED ST_LCP_CONFREQ_ASYNCMAP, *PST_LCP_CONFREQ_ASYNCMAP;

//* AUTHTYPE，认证类型，该配置项紧跟协议类型值之后为可变长数据域，指定具体协议携带的附加数据
typedef struct _ST_LCP_CONFREQ_AUTHTYPE_HDR_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	USHORT usType; //* 协议值：PPP_PAP/PPP_CHAP/其它
} PACKED ST_LCP_CONFREQ_AUTHTYPE_HDR, *PST_LCP_CONFREQ_AUTHTYPE_HDR;

//* QUALITY，链路质量监测协议，该配置项紧跟协议类型值之后为可变长数据域，指定具体协议携带的附加数据
typedef struct _ST_LCP_CONFREQ_QUALITY_HDR_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	USHORT usType; //* 协议值：PPP_LQR/其它
} PACKED ST_LCP_CONFREQ_QUALITY_HDR, *PST_LCP_CONFREQ_QUALITY_HDR;

//* MAGICNUMBER，魔术字，监测网络是否存在自环情形，即“短路”——自己发给自己
typedef struct _ST_LCP_CONFREQ_MAGICNUMBER_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	UINT unNum;
} PACKED ST_LCP_CONFREQ_MAGICNUMBER, *PST_LCP_CONFREQ_MAGICNUMBER;

//* PCOMPRESSION
typedef struct _ST_LCP_CONFREQ_PCOMPRESSION_ {
	ST_LNCP_CONFREQ_HDR stHdr;
} PACKED ST_LCP_CONFREQ_PCOMPRESSION, *PST_LCP_CONFREQ_PCOMPRESSION;

//* ACCOMPRESSION
typedef struct _ST_LCP_CONFREQ_ACCOMPRESSION_ {
	ST_LNCP_CONFREQ_HDR stHdr;
} PACKED ST_LCP_CONFREQ_ACCOMPRESSION, *PST_LCP_CONFREQ_ACCOMPRESSION;

//* CHAP协议的数据域结构
#define CHAP_CHALLENGE_LEN	16 //* CHAP协议固定使用MD5算法，挑战字符串的长度固定16个字节
typedef struct _ST_CHAP_DATA_ {
	UCHAR ubChallengeLen;
	UCHAR ubaChallenge[CHAP_CHALLENGE_LEN]; 	
} ST_CHAP_DATA, *PST_CHAP_DATA;

typedef enum {
	CHALLENGE = 1, 
	RESPONSE = 2, 
	SUCCESS = 3, 
	FAILURE = 4
} EN_CHAPCODE;

//* IPCP配置请求项相关宏及数据结构定义
#define IPCP_CONFREQ_NUM	5	//* 本程序支持的IPCP报文配置请求项的数量
typedef enum {
	ADDRS = 1,					//* IP-Addresses，IP地址选项配置，该选项因为具体应用存在问题现已被停用，只有对端强制要求时才会被启用
	COMPRESSION_PROTOCOL = 2,	//* IP-Compression-Protocol，指定要使用的压缩协议，缺省不进行压缩，所以这个程序同样不处理这个配置项
	ADDR = 3, 					//* IP-Addr，协商本地使用的IP地址，数据域全为0则要求对端给分配一个地址，此时对端会在CONFNAK帧中携带这个地址
	PRIMARYDNS = 129,			//* Primary DNS Server addr，协商主DNS服务器地址，数据域全为0则要求对端提供一个DNS地址，对端同样在CONFNAK帧中给出这个地址	
	SECONDARYDNS = 131			//* Secondary DNS server addr, 协商次DNS服务器地址，规则与上同
								//* 另，130和132用于协商NetBIOS网络节点的主、次地址，本系统不支持
} EN_IPCP_CONFREQ_TYPE;
typedef struct _ST_IPCP_CONFREQ_ADDR_ {
	ST_LNCP_CONFREQ_HDR stHdr;
	UINT unVal; //* 地址
} PACKED ST_IPCP_CONFREQ_ADDR, *PST_IPCP_CONFREQ_ADDR;
//* =======================================================================

//* 业务相关的基础数据结构定义
//* =======================================================================
typedef enum {
	MODEMRDY = 0, 
	LOWERUP,
	STARTNEGOTIATION, 
	NEGOTIATION, 
	IFUP, 
	ESTABLISHED
} EN_LINK_STATE;

typedef struct _ST_WA_LINKEDLISTNODE_ { //* 等待应答的报文链表节点
	_ST_WA_LINKEDLISTNODE_ *pstPrev;
	_ST_WA_LINKEDLISTNODE_ *pstNext;	
	struct {
		USHORT usProtocol; 
		UCHAR ubCode; 
		UCHAR ubIdentifier; 
	} stPacket;
	time_t tSend;		//* 报文的发送时间
	BOOL blIsAcked; 
	Timer *pobjTimer; 
} ST_WA_LINKEDLISTNODE, *PST_WA_LINKEDLISTNODE;

//* LNCP协议配置请求项处理器相关宏、处理函数及结构体定义
#define ACCM_INIT	0
typedef INT(*PFUN_FILLREQITEM)(UCHAR *pubFilled); 
typedef INT(*PFUN_GETREQVAL)(UCHAR *pubItem, UCHAR *pubVal);
typedef struct _ST_LNCP_CONFREQ_ITEM_ { 
	UCHAR ubType;	
	BOOL blIsNegoRequired;				//* 是否需要协商，生成初始配置请求报文时需要
	PFUN_FILLREQITEM pfunFillReqItem;	//* 填充请求内容到缓冲区，包括请求类型、长度及数据（如果需要携带数据的话）
	PFUN_GETREQVAL pfunGetReqVal;		//* 从收到的配置请求报文中读取协商值
} ST_LNCP_CONFREQ_ITEM, *PST_LNCP_CONFREQ_ITEM;
static INT FillReqItem_ASYNCMAP(UCHAR *pubFilled);
static INT FillReqItem_MAGICNUMBER(UCHAR *pubFilled);
static INT FillReqItem_PCOMPRESSION(UCHAR *pubFilled);
static INT FillReqItem_ACCOMPRESSION(UCHAR *pubFilled);
static INT GetReqVal_MRU(UCHAR *pubItem, UCHAR *pubVal);
static INT GetReqVal_ASYNCMAP(UCHAR *pubItem, UCHAR *pubVal);
static INT GetReqVal_AUTHTYPE(UCHAR *pubItem, UCHAR *pubVal);
static INT GetReqVal_MAGICNUMBER(UCHAR *pubItem, UCHAR *pubVal);
static INT GetReqVal_PCOMPRESSION(UCHAR *pubItem, UCHAR *pubVal);
static INT GetReqVal_ACCOMPRESSION(UCHAR *pubItem, UCHAR *pubVal);
static ST_LNCP_CONFREQ_ITEM l_staLCPConfReqItem[LCP_CONFREQ_NUM] =
{
	{ (UCHAR)EN_LCP_CONFREQ_TYPE::MRU,           FALSE, nullptr,				   GetReqVal_MRU },
	{ (UCHAR)EN_LCP_CONFREQ_TYPE::ASYNCMAP,      TRUE,  FillReqItem_ASYNCMAP,	   GetReqVal_ASYNCMAP },
	{ (UCHAR)EN_LCP_CONFREQ_TYPE::AUTHTYPE,      FALSE, nullptr,				   GetReqVal_AUTHTYPE },
	{ (UCHAR)EN_LCP_CONFREQ_TYPE::QUALITY,       FALSE, nullptr },
	{ (UCHAR)EN_LCP_CONFREQ_TYPE::MAGICNUMBER,   TRUE,  FillReqItem_MAGICNUMBER,   GetReqVal_MAGICNUMBER },
	{ (UCHAR)EN_LCP_CONFREQ_TYPE::PCOMPRESSION,  TRUE,  FillReqItem_PCOMPRESSION,  GetReqVal_PCOMPRESSION },
	{ (UCHAR)EN_LCP_CONFREQ_TYPE::ACCOMPRESSION, TRUE,  FillReqItem_ACCOMPRESSION, GetReqVal_ACCOMPRESSION }
}; 

//* LNCP协商处理器，其针对报文代码域携带的值分别进行特定处理，在这里定义处理器相关的基础数据结构、宏、处理函数等定义
typedef BOOL(*PFUN_LNCPNEGOHANDLER)(HANDLE& hDev, UCHAR *pubPacket, INT nPacketLen); 
typedef struct _ST_LCPNEGOHANDLER_ {
	EN_CPCODE enCode; 
	PFUN_LNCPNEGOHANDLER pfunHandler; 
} ST_LNCPNEGOHANDLER, *PST_LNCPNEGOHANDLER;
static BOOL LCPNegoHandler_CONFREQ(HANDLE& hPPP, UCHAR *pubPacket, INT nPacketLen);
static BOOL LCPNegoHandler_CONFACK(HANDLE& hPPP, UCHAR *pubPacket, INT nPacketLen);
static BOOL LCPNegoHandler_TERMACK(HANDLE& hPPP, UCHAR *pubPacket, INT nPacketLen);
static ST_LNCPNEGOHANDLER l_staLCPNegoHandler[CPCODE_NUM] = 
{
	{ CONFREQ, LCPNegoHandler_CONFREQ }, 
	{ CONFACK, LCPNegoHandler_CONFACK }, 
	{ CONFNAK, nullptr },
	{ CONFREJ, nullptr },
	{ TERMREQ, nullptr },
	{ TERMACK, LCPNegoHandler_TERMACK },
	{ CODEREJ, nullptr },
	{ PROTREJ, nullptr },
	{ ECHOREQ, nullptr },
	{ ECHOREP, nullptr },
	{ DISCREQ, nullptr }
}; 

//* IPCP相关配置项处理函数及宏定义
#define IP_ADDR_INIT	0
#define MASK_INIT		0xFFFFFFFF //* 对于版本大于2.1.16的内核，子网掩码强制为255.255.255.255
#define DNS_ADDR_INIT	0
static INT FillReqItem_ADDR(UCHAR *pubFilled);
static INT FillReqItem_PRIMARYDNS(UCHAR *pubFilled);
static INT FillReqItem_SECONDARYDNS(UCHAR *pubFilled);
static INT GetReqVal_ADDR(UCHAR *pubItem, UCHAR *pubVal);
static INT GetReqVal_PRIMARYDNS(UCHAR *pubItem, UCHAR *pubVal);
static INT GetReqVal_SECONDARYDNS(UCHAR *pubItem, UCHAR *pubVal);
static ST_LNCP_CONFREQ_ITEM l_staIPCPConfReqItem[IPCP_CONFREQ_NUM] =
{
	{ (UCHAR)EN_IPCP_CONFREQ_TYPE::ADDRS,				 FALSE, nullptr,				  nullptr },
	{ (UCHAR)EN_IPCP_CONFREQ_TYPE::COMPRESSION_PROTOCOL, FALSE, nullptr,				  nullptr },
	{ (UCHAR)EN_IPCP_CONFREQ_TYPE::ADDR,				 TRUE,  FillReqItem_ADDR,		  GetReqVal_ADDR },
	{ (UCHAR)EN_IPCP_CONFREQ_TYPE::PRIMARYDNS,			 TRUE,  FillReqItem_PRIMARYDNS,   GetReqVal_PRIMARYDNS },
	{ (UCHAR)EN_IPCP_CONFREQ_TYPE::SECONDARYDNS,		 TRUE,  FillReqItem_SECONDARYDNS, GetReqVal_SECONDARYDNS },
};

//* IPCP协商处理器
#define IPCPCODE_NUM	5	//* 协商阶段代码域类型数量
static BOOL IPCPNegoHandler_CONFREQ(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen);
static BOOL IPCPNegoHandler_CONFACK(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen);
static BOOL IPCPNegoHandler_CONFNAK(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen);
static BOOL IPCPNegoHandler_CONFREJ(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen);
static BOOL IPCPNegoHandler_CODEREJ(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen); 
static ST_LNCPNEGOHANDLER l_staIPCPNegoHandler[IPCPCODE_NUM] =
{
	{ CONFREQ, IPCPNegoHandler_CONFREQ }, 
	{ CONFACK, IPCPNegoHandler_CONFACK }, 
	{ CONFNAK, IPCPNegoHandler_CONFNAK }, 
	{ CONFREJ, IPCPNegoHandler_CONFREJ }, 
	{ CODEREJ, IPCPNegoHandler_CODEREJ } 
};

//* 记录协商结果
typedef struct _ST_NEGORESULT_ {
	struct {
		UINT unACCM;
		USHORT usMRU;
		struct {
			USHORT usType;
			UCHAR ubaData[16];
		} stAuth;
		BOOL blIsProtoComp;
		BOOL blIsAddrCtlComp;
	} stLCP;
	struct {
		UINT unAddr;
		UINT unPrimaryDNSAddr;
		UINT unSecondaryDNSAddr;
		UINT unPointToPointAddr; 
		UINT unNetMask; 
	} stIPCP;
} ST_NEGORESULT, *PST_NEGORESULT;

static ST_NEGORESULT l_stNegoResult =
{
	{ PPP_MRU, ACCM_INIT,{ PPP_CHAP, 0x05 /* 对于CHAP协议来说，0-4未使用，0x05代表采用MD5算法 */ }, TRUE, TRUE },
	{ IP_ADDR_INIT, DNS_ADDR_INIT, DNS_ADDR_INIT, IP_ADDR_INIT, MASK_INIT }
};

//* IPCP协商状态定义
typedef enum {
	REQADDR = 0, 
	RCVEDNAK, 
	CONFIRMADDR, 
	FINISHED
} EN_IPCPNEGOSTATE;
static EN_IPCPNEGOSTATE l_enIPCPNegoState = REQADDR;
//* =======================================================================

static THMutex *l_pobjLockWALinkedList = nullptr;
static BOOL l_blIsNeedToRecreatePPPLink = FALSE; 
static PST_WA_LINKEDLISTNODE l_pstWALinkedListHead = nullptr, l_pstWALinkedListTail = nullptr; 
static atomic_uchar l_atubLCPIdentifier(0), l_atubIPCPIdentifier(0);

static BOOL l_blIsRunning = TRUE;
void SYSClose(INT nSigID)	//* 捕获控制台ctrl+c输入，避免系统报错
{
	l_blIsRunning = FALSE;
	cout << "rcv exit signal" << endl;	
}

static void PrintHexArrayByStderr(const UCHAR *pubHexArray, USHORT usHexArrayLen, UCHAR ubBytesPerLine)
{
	INT i;

	fprintf(stderr, "%02X", pubHexArray[0]);
	for (i = 1; i < usHexArrayLen; i++)
	{
		if (i % (INT)ubBytesPerLine)
			fprintf(stderr, " ");
		else
			fprintf(stderr, "\r\n");
		fprintf(stderr, "%02X", pubHexArray[i]);
	}

	fprintf(stderr, "\r\n");
}

string GetProtocolName(USHORT usProtocol)
{
	switch (usProtocol)
	{
	case PPP_LCP:
		return "LCP";

	case PPP_CCP:
		return "CCP";

	case PPP_PAP:
		return "PAP";

	case PPP_CHAP:
		return "CHAP";

	case PPP_IPCP:
		return "IPCP";

	case PPP_IP:
		return "IP";

	case PPP_IPV6:
		return "IPv6";

	case PPP_LQR:
		return "LQR";

	default:
		return "Unrecognized";
	}
}

string GetCPCodeName(EN_CPCODE enCode)
{
	switch (enCode)
	{
	case CONFREQ:
		return "Configure Request";

	case CONFACK:
		return "Configure Ack";

	case CONFNAK:
		return "Configure Nak";

	case CONFREJ:
		return "Configure Reject";

	case TERMREQ:
		return "Terminate Request";

	case TERMACK:
		return "Terminate Ack";

	case CODEREJ:
		return "Code Reject";

	case PROTREJ:
		return "Protocol Reject";

	case ECHOREQ:
		return "Echo Request";

	case ECHOREP:
		return "Echo Reply";

	case DISCREQ:
		return "Discard Request";

	default:
		return "Unrecognized";
	}
}

string GetCHAPCodeName(EN_CHAPCODE enCode)
{
	switch (enCode)
	{
	case CHALLENGE:
		return "Challenge";

	case RESPONSE:
		return "Response"; 

	case SUCCESS:
		return "Success";

	case FAILURE:
		return "Failure";

	default:
		return "Unrecognized";
	}
}

static INT FillReqItem_ASYNCMAP(UCHAR *pubFilled)
{
	PST_LCP_CONFREQ_ASYNCMAP pstReq = (PST_LCP_CONFREQ_ASYNCMAP)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)EN_LCP_CONFREQ_TYPE::ASYNCMAP;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + 4;

	UINT unACCM = htonl((UINT)ACCM_INIT);
	fprintf(stderr, ", ACCM = %08X", unACCM); 
	memcpy((UCHAR *)&pstReq->unMap, (UCHAR *)&unACCM, 4);
	return (INT)pstReq->stHdr.ubLen;
}

static INT FillReqItem_MAGICNUMBER(UCHAR *pubFilled)
{
	PST_LCP_CONFREQ_MAGICNUMBER pstReq = (PST_LCP_CONFREQ_MAGICNUMBER)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)MAGICNUMBER; 
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + 4;

	UINT unMagicNum = htonl((UINT)toolsbox::random());
	memcpy((UCHAR *)&pstReq->unNum, &unMagicNum, 4);
	return (INT)pstReq->stHdr.ubLen;
}

static INT FillReqItem_PCOMPRESSION(UCHAR *pubFilled)
{
	PST_LCP_CONFREQ_PCOMPRESSION pstReq = (PST_LCP_CONFREQ_PCOMPRESSION)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)PCOMPRESSION;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR);

	fprintf(stderr, ", Protocol Field Compression");

	return (INT)pstReq->stHdr.ubLen;
}

static INT FillReqItem_ACCOMPRESSION(UCHAR *pubFilled)
{
	PST_LCP_CONFREQ_ACCOMPRESSION pstReq = (PST_LCP_CONFREQ_ACCOMPRESSION)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)ACCOMPRESSION;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR);

	fprintf(stderr, ", Address/Control Field Compression");

	return (INT)pstReq->stHdr.ubLen;
}

static INT GetReqVal_MRU(UCHAR *pubItem, UCHAR *pubVal)
{
	PST_LCP_CONFREQ_MRU pstItem = (PST_LCP_CONFREQ_MRU)pubItem;
	USHORT usVal = ntohs(pstItem->usMRU);
	if(pubVal)
		memcpy(pubVal, (UCHAR *)&usVal, sizeof(pstItem->usMRU));
	l_stNegoResult.stLCP.usMRU = usVal;

	fprintf(stderr, ", MRU = %d", usVal);

	return (INT)pstItem->stHdr.ubLen;
}

static INT GetReqVal_ASYNCMAP(UCHAR *pubItem, UCHAR *pubVal)
{
	PST_LCP_CONFREQ_ASYNCMAP pstItem = (PST_LCP_CONFREQ_ASYNCMAP)pubItem;
	UINT unVal = ntohl(pstItem->unMap);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&unVal, sizeof(pstItem->unMap));
	l_stNegoResult.stLCP.unACCM = unVal;

	fprintf(stderr, ", ACCM = %08X", unVal);

	return (INT)pstItem->stHdr.ubLen;
}

static INT GetReqVal_AUTHTYPE(UCHAR *pubItem, UCHAR *pubVal)
{
	PST_LCP_CONFREQ_AUTHTYPE_HDR pstItem = (PST_LCP_CONFREQ_AUTHTYPE_HDR)pubItem;
	USHORT usVal = ntohs(pstItem->usType);
	if (pubVal)
	{
		memcpy(pubVal, (UCHAR *)&usVal, sizeof(pstItem->usType));
		pubVal[sizeof(pstItem->usType)] = pubItem[sizeof(ST_LCP_CONFREQ_AUTHTYPE_HDR)];
	}
	l_stNegoResult.stLCP.stAuth.usType = usVal; 
	memcpy(l_stNegoResult.stLCP.stAuth.ubaData, pubItem + sizeof(ST_LCP_CONFREQ_AUTHTYPE_HDR), (size_t)pstItem->stHdr.ubLen - sizeof(ST_LCP_CONFREQ_AUTHTYPE_HDR));
	fprintf(stderr, ", Authentication type = '%s'", GetProtocolName(usVal).c_str());

	return (INT)pstItem->stHdr.ubLen;
}

INT GetReqVal_MAGICNUMBER(UCHAR *pubItem, UCHAR *pubVal)
{
	PST_LCP_CONFREQ_MAGICNUMBER pstItem = (PST_LCP_CONFREQ_MAGICNUMBER)pubItem;
	UINT unVal = ntohl(pstItem->unNum);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&unVal, sizeof(pstItem->unNum));	

	return (INT)pstItem->stHdr.ubLen;
}

static INT GetReqVal_PCOMPRESSION(UCHAR *pubItem, UCHAR *pubVal)
{	
	PST_LCP_CONFREQ_PCOMPRESSION pstItem = (PST_LCP_CONFREQ_PCOMPRESSION)pubItem;
	l_stNegoResult.stLCP.blIsProtoComp = TRUE;

	fprintf(stderr, ", Protocol Field Compression");

	return (INT)pstItem->stHdr.ubLen;
}

static INT GetReqVal_ACCOMPRESSION(UCHAR *pubItem, UCHAR *pubVal)
{	
	PST_LCP_CONFREQ_ACCOMPRESSION pstItem = (PST_LCP_CONFREQ_ACCOMPRESSION)pubItem;
	l_stNegoResult.stLCP.blIsAddrCtlComp = TRUE;

	fprintf(stderr, ", Address/Control Field Compression");

	return (INT)pstItem->stHdr.ubLen;
}

static INT FillReqItem_ADDR(UCHAR *pubFilled)
{
	PST_IPCP_CONFREQ_ADDR pstReq = (PST_IPCP_CONFREQ_ADDR)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)EN_IPCP_CONFREQ_TYPE::ADDR;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + 4;
	
	//UINT unAddr = htonl(l_stNegoResult.stIPCP.unAddr);
	struct in_addr stAddr = { l_stNegoResult.stIPCP.unAddr };
	fprintf(stderr, ", IP <%s>", inet_ntoa(stAddr));
	memcpy((UCHAR *)&pstReq->unVal, (UCHAR *)&l_stNegoResult.stIPCP.unAddr, 4);
	return (INT)pstReq->stHdr.ubLen;
}

static INT FillReqItem_PRIMARYDNS(UCHAR *pubFilled)
{
	PST_IPCP_CONFREQ_ADDR pstReq = (PST_IPCP_CONFREQ_ADDR)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)EN_IPCP_CONFREQ_TYPE::PRIMARYDNS;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + 4;
	
	//UINT unAddr = htonl(l_stNegoResult.stIPCP.unPrimaryDNSAddr);
	struct in_addr stAddr = { l_stNegoResult.stIPCP.unPrimaryDNSAddr };
	fprintf(stderr, ", Primary DNS <%s>", inet_ntoa(stAddr));
	memcpy((UCHAR *)&pstReq->unVal, (UCHAR *)&l_stNegoResult.stIPCP.unPrimaryDNSAddr, 4);
	return (INT)pstReq->stHdr.ubLen;
}

static INT FillReqItem_SECONDARYDNS(UCHAR *pubFilled)
{
	PST_IPCP_CONFREQ_ADDR pstReq = (PST_IPCP_CONFREQ_ADDR)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)EN_IPCP_CONFREQ_TYPE::SECONDARYDNS;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + 4;
		
	//UINT unAddr = htonl(l_stNegoResult.stIPCP.unSecondaryDNSAddr);
	struct in_addr stAddr = { l_stNegoResult.stIPCP.unSecondaryDNSAddr };
	fprintf(stderr, ", Secondary DNS <%s>", inet_ntoa(stAddr));
	memcpy((UCHAR *)&pstReq->unVal, (UCHAR *)&l_stNegoResult.stIPCP.unSecondaryDNSAddr, 4);
	return (INT)pstReq->stHdr.ubLen;
}

static INT GetReqVal_ADDR(UCHAR *pubItem, UCHAR *pubVal)
{
	PST_IPCP_CONFREQ_ADDR pstItem = (PST_IPCP_CONFREQ_ADDR)pubItem;
	//UINT unVal = ntohl(pstItem->unVal);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&pstItem->unVal, sizeof(pstItem->unVal));
	l_stNegoResult.stIPCP.unAddr = pstItem->unVal;

	struct in_addr stAddr = { pstItem->unVal };
	fprintf(stderr, ", IP <%s> ", inet_ntoa(stAddr));

	return (INT)pstItem->stHdr.ubLen;
}

static INT GetReqVal_PRIMARYDNS(UCHAR *pubItem, UCHAR *pubVal)
{
	PST_IPCP_CONFREQ_ADDR pstItem = (PST_IPCP_CONFREQ_ADDR)pubItem;
	//UINT unVal = ntohl(pstItem->unVal);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&pstItem->unVal, sizeof(pstItem->unVal));
	l_stNegoResult.stIPCP.unPrimaryDNSAddr = pstItem->unVal;

	struct in_addr stAddr = { pstItem->unVal };
	fprintf(stderr, ", Primary DNS <%s>", inet_ntoa(stAddr));

	return (INT)pstItem->stHdr.ubLen;
}

static INT GetReqVal_SECONDARYDNS(UCHAR *pubItem, UCHAR *pubVal)
{
	PST_IPCP_CONFREQ_ADDR pstItem = (PST_IPCP_CONFREQ_ADDR)pubItem;
	//UINT unVal = ntohl(pstItem->unVal);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&pstItem->unVal, sizeof(pstItem->unVal));
	l_stNegoResult.stIPCP.unSecondaryDNSAddr = pstItem->unVal;

	struct in_addr stAddr = { pstItem->unVal };
	fprintf(stderr, ", Secondary DNS <%s>", inet_ntoa(stAddr));

	return (INT)pstItem->stHdr.ubLen;
}

static void FreeNodeOfWALinkedList(PST_WA_LINKEDLISTNODE pstFreedNode); 
void WATimerHandler(union sigval uniSigVal)
{
	PST_WA_LINKEDLISTNODE pstTimeoutNode = (PST_WA_LINKEDLISTNODE)uniSigVal.sival_ptr;	

	if (pstTimeoutNode->blIsAcked) //* 如果已经收到应答报文，则不需要发送链路重建通知，仅释放当前节点即可
		FreeNodeOfWALinkedList(pstTimeoutNode);
	else //* 超时了，当前节点记录的报文未收到应答
	{
		l_pobjLockWALinkedList->lock();
		{
			if (pstTimeoutNode->blIsAcked) //* 存在这种情况，等待进入临界段时另一个线程收到应答报文了
			{
				l_pobjLockWALinkedList->unlock();

				FreeNodeOfWALinkedList(pstTimeoutNode);
				return; 
			}

			//* 销毁所有尚未等到应答的节点资源，避免内存泄露
			PST_WA_LINKEDLISTNODE pstPrev = l_pstWALinkedListTail;
			while (pstPrev)
			{
				PST_WA_LINKEDLISTNODE pstCurNode = pstPrev;
				pstPrev = pstPrev->pstPrev;  //* 移动到前一个节点
				delete pstCurNode;			 //* 删除当前节点
			}
			l_pstWALinkedListHead = l_pstWALinkedListTail = nullptr;

			//* 通知主状态机重建PPP链路，从最原始的modem拨号开始
			l_blIsNeedToRecreatePPPLink = TRUE;			
		}
		l_pobjLockWALinkedList->unlock();		

		fprintf(stderr, "接收超时 <Protocol %s, Code = '%s', Identifier = %d>\r\n", 
					GetProtocolName(pstTimeoutNode->stPacket.usProtocol).c_str(), 
					GetCPCodeName((EN_CPCODE)pstTimeoutNode->stPacket.ubCode).c_str(), (UINT)pstTimeoutNode->stPacket.ubIdentifier);
	}
}

//* 将需要等待应答的报文放入链表
BOOL PutIntoWALinkedList(USHORT usProtocol, UCHAR ubCode, UCHAR ubIdentifier, INT nTimeout)
{
	PST_WA_LINKEDLISTNODE pstNewNode = new(std::nothrow) ST_WA_LINKEDLISTNODE;
	if (!pstNewNode)
	{
		fprintf(stderr, "为等待应答报文链表新增节点失败，%s，错误码：%d\r\n", strerror(errno), errno);
		return FALSE;
	}

	//* 填充数据
	pstNewNode->stPacket.usProtocol   = usProtocol; 
	pstNewNode->stPacket.ubCode       = ubCode; 
	pstNewNode->stPacket.ubIdentifier = ubIdentifier; 
	pstNewNode->blIsAcked             = FALSE; 
	pstNewNode->tSend                 = time(nullptr);

	//* 建立一个定时器
	pstNewNode->pobjTimer = new(std::nothrow) Timer(WATimerHandler, nTimeout, 0, 0, 0, pstNewNode);
	if (!pstNewNode->pobjTimer)
	{		
		fprintf(stderr, "为等待应答报文链表新增节点失败，%s，错误码：%d\r\n", strerror(errno), errno);
		delete pstNewNode;
		return FALSE;
	}

	pstNewNode->pstNext = nullptr;
	l_pobjLockWALinkedList->lock();
	{
		if (l_pstWALinkedListHead)
		{
			pstNewNode->pstPrev            = l_pstWALinkedListTail; 
			l_pstWALinkedListTail->pstNext = pstNewNode;
			l_pstWALinkedListTail          = pstNewNode; 
		}
		else
		{
			pstNewNode->pstPrev   = nullptr;
			l_pstWALinkedListHead = pstNewNode;
			l_pstWALinkedListTail = pstNewNode; 
		}
	}
	l_pobjLockWALinkedList->unlock();

	return TRUE; 
}

static void FreeNodeOfWALinkedList(PST_WA_LINKEDLISTNODE pstFreedNode)
{
	l_pobjLockWALinkedList->lock();
	{
		if (pstFreedNode)
		{
			//* 从链表中摘除
			if (pstFreedNode->pstPrev)			
				pstFreedNode->pstPrev->pstNext = pstFreedNode->pstNext; 
			if (pstFreedNode->pstNext)
				pstFreedNode->pstNext->pstPrev = pstFreedNode->pstPrev;

			if (l_pstWALinkedListHead == pstFreedNode)
				l_pstWALinkedListHead = pstFreedNode->pstNext; 

			if (l_pstWALinkedListTail == pstFreedNode)
				l_pstWALinkedListTail = pstFreedNode->pstPrev;			
		}		
	}
	l_pobjLockWALinkedList->unlock();

	//* 回收资源
	delete pstFreedNode->pobjTimer; 
	delete pstFreedNode;
}

PST_WA_LINKEDLISTNODE FindWANode(USHORT usProtocol, UCHAR ubIdentifier)
{
	PST_WA_LINKEDLISTNODE pstNext = l_pstWALinkedListHead; 
	while (pstNext)
	{
		if (usProtocol == pstNext->stPacket.usProtocol && ubIdentifier == pstNext->stPacket.ubIdentifier)
			return pstNext; 

		pstNext = pstNext->pstNext; 
	}

	return nullptr; 
}

INT ATInstExec(const CHAR *pszAT, UINT unATBytes, const CHAR *pszOK, CHAR bOKBytes, CHAR *pszDataBuf, UINT unBufBytes, UINT unWaitSecs)
{
	INT nRcvBytes;
	CHAR szRcvBuf[1024] = { 0 };
	UINT unWaitTimes;
	INT nHaveRcvBytes, nHaveCpyBytes, nCpyBytes;

	//* 去掉尾部的回车换行，出错时使用
	string strATCmd(pszAT);
	strATCmd = strATCmd.substr(0, strATCmd.length() - 2);

	//* 发送AT指令	
	if (write(STDOUT_FILENO, (UCHAR *)pszAT, unATBytes) < 0)
	{
		fprintf(stderr, "<0>AT指令[%s]执行错误, %s\r\n", strATCmd.c_str(), strerror(errno));
		return -1;
	}

	Sleep(1);

	unWaitTimes = 0;
	nHaveRcvBytes = 0;
	nHaveCpyBytes = 0;
	while (unWaitTimes < unWaitSecs && nHaveRcvBytes <= (INT)sizeof(szRcvBuf) - 1)
	{
		//* 读指令执行结果		
		nRcvBytes = read(STDIN_FILENO, (UCHAR *)&szRcvBuf[nHaveRcvBytes], sizeof(szRcvBuf) - 1 - nHaveRcvBytes);

		//* 如果接收到了AT设备输出的操作结果字符串，则判断是正确还是错误
		if (nRcvBytes > 0)
		{
			//fprintf(stderr, "[%s] %d\r\n", strATCmd.c_str(), nRcvBytes);

			//* 如果不为空表明上层调用函数需要读取接收到的结果数据
			if (pszDataBuf != NULL)
			{
				if (nHaveCpyBytes < (INT)unBufBytes)
				{
					nCpyBytes = (((INT)unBufBytes - nHaveCpyBytes) > nRcvBytes) ? nRcvBytes : ((INT)unBufBytes - nHaveCpyBytes);
					strncpy((char*)pszDataBuf + nHaveCpyBytes, (char*)&szRcvBuf[nHaveRcvBytes], nCpyBytes);
					nHaveCpyBytes += nCpyBytes;
				}
			}
			else;

			nHaveRcvBytes += nRcvBytes;

			if (MemStr(szRcvBuf, pszOK, bOKBytes, nHaveRcvBytes) != 0)
				return 0;
		}
		else
		{
			if (nRcvBytes < 0)
			{
				fprintf(stderr, "<1>AT指令[%s]执行错误, %s\r\n", strATCmd.c_str(), strerror(errno));
				return -1;
			}
		}

		unWaitTimes++;
	}

	fprintf(stderr, "<2>AT指令[%s]执行超时\r\n", strATCmd.c_str());
	return -1;
}

BOOL chat(void)
{
	INT nRtnVal;
	CHAR szRcvBuf[1024] = { 0 };
	nRtnVal = ATInstExec("AT\r\n", sizeof("AT\r\n") - 1, "OK", 2, szRcvBuf, sizeof(szRcvBuf), 3);
	if (nRtnVal < 0)
		return FALSE;
	else
		fprintf(stderr, "%s", szRcvBuf);

	nRtnVal = ATInstExec("ATE0\r\n", sizeof("ATE0\r\n") - 1, "OK", 2, szRcvBuf, sizeof(szRcvBuf), 3);
	if (nRtnVal < 0)
		return FALSE;
	else
		fprintf(stderr, "%s", szRcvBuf);

	nRtnVal = ATInstExec("ATDT*99***1#\r\n", sizeof("ATDT*99***1#\r\n") - 1, "CONNECT", 7, szRcvBuf, sizeof(szRcvBuf), 3);
	if (nRtnVal < 0)
		return FALSE;
	else
		fprintf(stderr, "%s", szRcvBuf);	

	/*
	Sleep(15);
	memset(szRcvBuf, 0, sizeof(szRcvBuf));
	INT nRcvBytes = read(STDIN_FILENO, (UCHAR *)&szRcvBuf, sizeof(szRcvBuf));
	fprintf(stderr, "nRcvBytes: %d\r\n", nRcvBytes);
	if (nRcvBytes > 0)
		fprintf(stderr, "%s\r\n", szRcvBuf);
	else
		fprintf(stderr, "not received any bytes\r\n"); 
	*/

	return TRUE;
}

//* 确保内核相关配置就绪
void KernelReady(HANDLE& hPPP, HANDLE& hPPPDev)
{
	ext_accm unaExtAccm = { 0 };

	/*
	unsigned char *pubAccm = (unsigned char *)&unaExtAccm;
	for (size_t i = 0; i<sizeof(unaExtAccm); i++)
		fprintf(stderr, "<0>ext_accm[%d] %02X\r\n", i, pubAccm[i]);
	*/

	//* 查看内核源码drivers\net\ppp\ppp_async.c中ppp_async_ioctl函数的PPPIOCSXASYNCMAP分支可以看出：
	//* 每一位代表一个要转义的ASCII字符，32位整形数组xmit_accm[3]正好指代ASCII表的96到127字符，0x60位
	//* 于该整形数组单元xmit_accm[3]的最高字节，也就是代表ASCII字符的120到127，0x60是将第5、6位置1，也
	//* 就是转义ASCII的125和126字符，也就是PPP帧中必须要转义的0x7d, 0x7e
	unaExtAccm[3] = 0x60000000; 
	if (ioctl(hPPP, PPPIOCSXASYNCMAP, unaExtAccm) < 0 && errno != ENOTTY && errno != EIO)	
			fprintf(stderr, "调整ppp模块内核配置项之扩展ACCM失败, %s, 错误码: %d\r\n", strerror(errno), errno);	

	/*
	// 测试代码，确保扩展ACCM已经写入内核ppp模块配置
	unaExtAccm[3] = 0x00000000;
	if (ioctl(hPPP, PPPIOCGXASYNCMAP, unaExtAccm) >= 0)
	{
		for (size_t i = 0; i<sizeof(unaExtAccm); i++)
			fprintf(stderr, "<1>ext_accm[%d] %02X\r\n", i, pubAccm[i]);
	}
	*/

	//* 异步链路中，有些串行接口驱动程序或modem需要用一些字符作为控制字符使用，字符的范围是0x00－0x1F（0-31）所以需要对这些字符
	//* 进行转义处理，大多数情况下并不是这个范围内的所有字符都作为控制字符，因此PPP LCP提供这个选项用来告诉对端，本端将哪些字符
	//* 作为控制字符处理，请对端将这些字符转义后发送；
	//* 参考内核源码ppp_async.c中ppp_async_ioctl函数PPPIOCSASYNCMAP分支，其设置第一组ACCM，也就是上面说的ASCII字符集前面32个
	//* 字符全部转义，这样无论是什么样的tty驱动及modem，其使用这个范围内的任何字符进行控制也不会与PPP帧冲突了；
	//* 经过实际测试发现，其实不需要设置转义也没问题，也就是unAsyncmap全为0也可
	UINT unAsyncmap = 0xFFFFFFFF;
	if (ioctl(hPPP, PPPIOCSASYNCMAP, (caddr_t)&unAsyncmap) < 0 && errno != EIO && errno != ENOTTY)
			fprintf(stderr, "设置传输字符转义选项（ACCM）失败, %s, 错误码: %d\r\n", strerror(errno), errno);	
	/*
	// 测试代码，确定ACCM写入成功
	unAsyncmap = 0;
	ioctl(hPPP, PPPIOCGASYNCMAP, (caddr_t)&unAsyncmap); 
	fprintf(stderr, "%08X\r\n", unAsyncmap);
	*/

	//* 发送的报文不压缩，清除掉内核的相关压缩标志
	INT nFlags;
	if (ioctl(hPPP, PPPIOCGFLAGS, &nFlags) >= 0)
	{
		INT nClearBits = SC_COMP_PROT | SC_COMP_AC | SC_SYNC; 
		nFlags = (nFlags & ~nClearBits);
		if (ioctl(hPPP, PPPIOCSFLAGS, &nFlags) < 0)
			fprintf(stderr, "<LowerLayerReady>设置内核控制标志位失败, %s, 错误码: %d\r\n", strerror(errno), errno);
	}
	else
		fprintf(stderr, "<LowerLayerReady>读取内核控制标志位失败, %s, 错误码: %d\r\n", strerror(errno), errno);

	//* 设置ppp通道的最大接收单元值（set channel receive MRU）
	INT nMRU = PPP_MRU;
	if (ioctl(hPPP, PPPIOCSMRU, (caddr_t)&nMRU) < 0 && errno != EIO && errno != ENOTTY)
		fprintf(stderr, "设置ppp通道的最大接收单元（PPPIOCSMRU）失败, %s, 错误码: %d\r\n", strerror(errno), errno);

	//* 设置通用ppp层的最大接收单元（set MRU in generic PPP layer）
	if(ioctl(hPPPDev, PPPIOCSMRU, (caddr_t)&nMRU) < 0)
		fprintf(stderr, "设置通用ppp层的最大接收单元（PPPIOCSMRU）失败, %s, 错误码: %d\r\n", strerror(errno), errno);

	//* 设置接收器的ACCM
	if (ioctl(hPPP, PPPIOCSRASYNCMAP, (caddr_t)&unAsyncmap) < 0 && errno != EIO && errno != ENOTTY)
		fprintf(stderr, "设置接收字符转义选项（ACCM）失败, %s, 错误码: %d\r\n", strerror(errno), errno);
	// 测试代码，确定ACCM写入成功
	unAsyncmap = 0;
	ioctl(hPPP, PPPIOCGRASYNCMAP, (caddr_t)&unAsyncmap);
	//fprintf(stderr, "Asyncmap: %08X\r\n", unAsyncmap);
}

BOOL EnterPPPMode(HANDLE& hPort, INT& nChannelIdx, HANDLE& hPPP, HANDLE& hPPPDev, INT& nIfUnit, fd_set& fdsIn, HANDLE& hMaxIn, string& strIfName, string& strPIDFile)
{
	if (ioctl(hPort, TIOCEXCL, 0) < 0)  //* 设置为tty终端为串行线路专用模式
	{
		if (errno != EIO)
		{
			fprintf(stderr, "将modem通讯口设置为串行线路专用模式失败, %s, 错误码: %d\r\n", strerror(errno), errno);
			return FALSE;
		}
	}

	//* linux/tty.h中包含N_PPP的定义，其为linux线路规程（line disciplines）机制的ppp相关的规程类型
	//* 线路规程的作用就是将数据进行用户空间和内核空间之间的传输，而一些底层驱动和终端驱动主要负责数据到硬件的传输。
	//* 往往不同的驱动对应着不同的线路规程，比如终端驱动对应着N_TTY ，点点通信对应着N_PPP等
	//* N_PPP: 异步规程，N_SYNC_PPP：同步规程，对于普通串口设备使用异步PPP规程
	//* http://blog.chinaunix.net/uid-20221192-id-2981668.html 关于ppp相关介绍：
	//* ppp设备是指在点对点的物理链路之间使用PPP帧进行分组交换的内核网络接口设备,由于Linux内核将串行设备作为终端设
	//* 备来驱动,于是引入PPP终端规程来实现终端设备与PPP设备的接口.根据终端设备的物理传输特性的不同,PPP规程分为异步
	//* 规程(N_PPP)和同步规程(N_SYNC_PPP)两种, 对于普通串口设备使用异步PPP规程；	
	//* 在PPP驱动程序中, 每一tty终端设备对应于一条PPP传输通道(channel),每一ppp网络设备对应于一个PPP接口单元(unit).
	//* 从终端设备上接收到的数据流通过PPP传输通道解码后转换成PPP帧传递到PPP网络接口单元,PPP接口单元再将PPP帧转换为PPP设备的接收帧. 
	//* 反之, 当PPP设备发射数据帧时,发射帧通过PPP接口单元转换成PPP帧传递给PPP通道,PPP通道负责将PPP帧编码后写入终端设备.
	//* 在配置了多链路PPP时(CONFIG_PPP_MULTILINK), 多个PPP传输通道可连接到同一PPP接口单元.PPP接口单元将PPP帧分割成若干个片段传递给
	//* 不同的PPP传输通道, 反之,PPP传输通道接收到的PPP帧片段被PPP接口单元重组成完整的PPP帧.
	//* 用户可以用ioctl(PPPIOCATTACH)将tty设备绑定到PPP接口单元上, 来读写PPP接口单元的输出帧,也可以用ioctl(PPPIOCATTCHAN)将tty设备绑定到
	//* PPP传输通道上, 来读写PPP传输通道的输入帧.
	//* 当终端设备的物理链路连接成功后, 用户使用ioctl(TIOCSETD)将终端切换到PPP规程.PPP规程初始化时, 将建立终端设备的传输通道和通道驱动结
	//* 构. 对于异步PPP规程来说,通道驱动结构为asyncppp, 它包含通道操作表async_ops.
	INT nLineDiscip = N_PPP;
	if (ioctl(hPort, TIOCSETD, &nLineDiscip) < 0) //* 设置tty端口的线路规程
	{
		if (errno != EIO)
		{
			fprintf(stderr, "设置modem口线路规程为ppp类型失败, %s, 错误码: %d\r\n", strerror(errno), errno);
			return FALSE;
		}
	}

	//* 将modem通讯口连接到linux内核提供的ppp通道上
	if (ioctl(hPort, PPPIOCGCHAN, &nChannelIdx) == -1)
	{
		fprintf(stderr, "无法获取可用的PPP通道号, %s, 错误码: %d\r\n", strerror(errno), errno);
		return FALSE;
	}

	//* 打开ppp设备节点
	hPPP = open("/dev/ppp", O_RDWR);
	if (hPPP < 0)
	{
		fprintf(stderr, "<0>无法打开设备节点/dev/ppp, %s, 错误码: %d\r\n", strerror(errno), errno);
		return FALSE;
	}
	fcntl(hPPP, F_SETFD, FD_CLOEXEC); //* 确保打开的节点在fork子进程时，子进程一旦执行exec函数族，子进程获得的该句柄副本就会被系统自动关闭
	if (ioctl(hPPP, PPPIOCATTCHAN, &nChannelIdx) < 0) //* 将modem通讯通道附着到ppp设备上
	{
		close(hPPP);

		fprintf(stderr, "无法将通道[%d]附着到ppp设备上, %s, 错误码: %d\r\n", nChannelIdx, strerror(errno), errno);
		return FALSE;
	}
	//* 设置为非阻塞
	INT nFlags = fcntl(hPPP, F_GETFL);
	if (nFlags == -1 || fcntl(hPPP, F_SETFL, nFlags | O_NONBLOCK) == -1)
		fprintf(stderr, "<0>修改设备节点/dev/ppp为非阻塞访问失败, %s, 错误码: %d\r\n", strerror(errno), errno);

	//* 再次打开ppp设备节点，以便新建一个ppp单元
	hPPPDev = open("/dev/ppp", O_RDWR);
	if (hPPPDev < 0)
	{
		close(hPPP);

		fprintf(stderr, "<1>无法打开设备节点/dev/ppp, %s, 错误码: %d\r\n", strerror(errno), errno);
		return FALSE;
	}
	//* 设置为非阻塞
	nFlags = fcntl(hPPPDev, F_GETFL);
	if (nFlags == -1 || fcntl(hPPPDev, F_SETFL, nFlags | O_NONBLOCK) == -1)
		fprintf(stderr, "<1>修改设备节点/dev/ppp为非阻塞访问失败, %s, 错误码: %d\r\n", strerror(errno), errno);
	nIfUnit = -1; //* 由系统分配，而不是指定一个
	if (ioctl(hPPPDev, PPPIOCNEWUNIT, &nIfUnit) < 0) //* 建立一个新的ppp单元
	{
		close(hPPP);

		fprintf(stderr, "ppp单元分配失败, %s, 错误码: %d\r\n", strerror(errno), errno);
		return FALSE;
	}
	//* 两个句柄加入FD_SET集合
	if (hPPP >= FD_SETSIZE)
	{
		close(hPPP);
		close(hPPPDev);

		fprintf(stderr, "<0>ppp设备句柄%d过大，超出了FD_SETSIZE宏指定的最大%d的限制，无法将其加入FD_SET集合\r\n", hPPP, FD_SETSIZE);
		return FALSE;
	}
	FD_SET(hPPP, &fdsIn);
	if (hPPP > hMaxIn)
		hMaxIn = hPPP;
	if (hPPPDev >= FD_SETSIZE)
	{
		close(hPPP);
		close(hPPPDev);

		fprintf(stderr, "<1>ppp设备句柄%d过大，超出了FD_SETSIZE宏指定的最大%d的限制，无法将其加入FD_SET集合\r\n", hPPPDev, FD_SETSIZE);
		return FALSE;
	}
	FD_SET(hPPPDev, &fdsIn);
	if (hPPPDev > hMaxIn)
		hMaxIn = hPPPDev; 

	//* 将新建立的ppp单元与ppp通道连接起来，其实就是：
	//* modem通道(tty串行口)—>/dev/ppp—>ppp unit
	if (ioctl(hPPP, PPPIOCCONNECT, &nIfUnit) < 0)
	{
		close(hPPP);
		close(hPPPDev);

		fprintf(stderr, "无法将通道[%d]附着到ppp单元%d上, %s, 错误码: %d\r\n", nChannelIdx, nIfUnit, strerror(errno), errno);
		return FALSE;
	}

	//* 通知内核打印调试信息
	INT nDebugLevel = 0;
	ioctl(hPPPDev, PPPIOCSDEBUG, &nDebugLevel);

	//* 设置并清除内核控制标志位
#define SC_RCVB	(SC_RCV_B7_0 | SC_RCV_B7_1 | SC_RCV_EVNP | SC_RCV_ODDP)
#define SC_LOGB	(SC_DEBUG | SC_LOG_INPKT | SC_LOG_OUTPKT | SC_LOG_RAWIN | SC_LOG_FLUSH)
	if (ioctl(hPPP, PPPIOCGFLAGS, &nFlags) >= 0)
	{
		INT nClearBits = SC_RCVB | SC_LOGB;
		nFlags = (nFlags & ~nClearBits);
		if (ioctl(hPPP, PPPIOCSFLAGS, &nFlags) < 0)		
			fprintf(stderr, "设置内核控制标志位失败, %s, 错误码: %d\r\n", strerror(errno), errno);		
	}
	else
		fprintf(stderr, "读取内核控制标志位失败, %s, 错误码: %d\r\n", strerror(errno), errno);

	//* 生成ppp接口名称
	ostringstream oss;
	oss << "ppp" << nIfUnit;
	strIfName = oss.str();

	//* pid写入文件，这是linux的ppp模块的标准做法，以供其他进程、脚本使用
	oss.str("");
	oss << "/var/run/" << strIfName << ".pid";
	strPIDFile = oss.str(); 
	FILE *pfPID = fopen(oss.str().c_str(), "w");
	if (pfPID)
	{
		fprintf(pfPID, "%d\n", getpid());
		fclose(pfPID);
	}
	else	
		fprintf(stderr, "为ppp进程准备的pid文件%s建立失败, %s, 错误码: %d\r\n", oss.str().c_str(), strerror(errno), errno);	

	//* 内核相关配置就绪
	KernelReady(hPPP, hPPPDev);

	fprintf(stderr, "The modem enters ppp mode.\r\nuse channel %d\r\npid file %s\r\n", nChannelIdx, strPIDFile.c_str());
	fprintf(stderr, "%s <--> %s\r\n", strIfName.c_str(), PPP_DEV_PORT);	

	return TRUE;
}

/*
void SendPacket(HANDLE& hPPP, HANDLE& hPPPDev)
{
	INT nWaitSecs = 0;
	UCHAR ubaRcvBuf[PPP_MRU + 64] = { 0 };
	INT nRcvBytes;

	UCHAR ubaPacket[PPP_MRU + PPP_HDRLEN] = "\xC0\x21\x01\x01\x00\x14\x02\x06\x00\x00\x00\x00\x05\x06\xB1\x82\x6E\x7F\x07\x02\x08\x02";
	UINT unMagicNum = (UINT)toolsbox::random();
	memcpy(&ubaPacket[14], &unMagicNum, 4);
	PrintHexArrayByStderr(ubaPacket, 22, 32);
	if (write(hPPP, ubaPacket, 22) < 0)
	{
		fprintf(stderr, "发送失败, %s, 错误码: %d\r\n", strerror(errno), errno);
		return;
	}	
	
__lblRcv: 
	if (nWaitSecs > 10)
	{
		fprintf(stderr, "接收超时\r\n");
		return; 
	}
	
	nRcvBytes = read(hPPP, (UCHAR *)&ubaRcvBuf, sizeof(ubaRcvBuf));
	fprintf(stderr, "nRcvBytes: %d\r\n", nRcvBytes);
	if (nRcvBytes > 0)
	{
		PrintHexArrayByStderr(ubaRcvBuf, nRcvBytes, 32);
	}
	else
	{
		Sleep(1);
		nWaitSecs++;

		goto __lblRcv;
	}
}
*/

BOOL SendPacket(HANDLE& hDev, UCHAR *pubPacket, USHORT usPacketLen)
{		
	INT nWriteBytes;
	if ((nWriteBytes = write(hDev, pubPacket, usPacketLen)) < 0)
	{
		fprintf(stderr, "数据发送失败, %s, 错误码: %d\r\n<%d>sent %d bytes: ", strerror(errno), errno, hDev, usPacketLen);
		PrintHexArrayByStderr(pubPacket, usPacketLen, 32);
		return FALSE;;
	}

#if DEBUG_LEVEL
	fprintf(stderr, "<%d>sent %d bytes: ", hDev, nWriteBytes);
	PrintHexArrayByStderr(pubPacket, nWriteBytes, 32);
#endif

	return TRUE;
}
 
BOOL LNCPSendPacket(HANDLE& hDev, USHORT usProtocol, UCHAR ubCode, UCHAR ubIdentifier, UCHAR *pubData, USHORT usDataLen, BOOL blIsWaitACK)
{
	//* 封装完整报文到发送缓冲区	
	UCHAR ubaPacket[sizeof(ST_PPP_HDR) + PPP_MRU + sizeof(ST_PPP_TAIL)];
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)ubaPacket;
	pstHdr->usProtocol      = htons(usProtocol);
	pstHdr->ubCode          = ubCode;
	pstHdr->ubIdentifier    = ubIdentifier;
	pstHdr->usLen           = htons((USHORT)(sizeof(ST_LNCP_HDR) + usDataLen));
	memcpy(&ubaPacket[sizeof(ST_LNCP_ACFC_HDR)], pubData, usDataLen);

	//* 发送
	SendPacket(hDev, ubaPacket, sizeof(ST_LNCP_ACFC_HDR) + usDataLen); 

	//* 如果需要等待应答则将其立即放到等待应答链表中
	if (blIsWaitACK)
		PutIntoWALinkedList(usProtocol, pstHdr->ubCode, pstHdr->ubIdentifier, 6);

	return TRUE; 
}

BOOL StartNegotiation(HANDLE& hPPP)
{
	l_pobjLockWALinkedList = new(std::nothrow) THMutex();
	if (!l_pobjLockWALinkedList)
	{
		fprintf(stderr, "为等待应答报文链表建立互斥锁失败，%s，错误码：%d\r\n", strerror(errno), errno);
		return FALSE; 
	}

	UCHAR ubIdentifire = l_atubLCPIdentifier++;
	fprintf(stderr, "sent [Protocol LCP, Id = %02X, Code = 'Configure Request'", ubIdentifire);
	UCHAR ubaConfReq[128]; 
	UINT unWriteIdx = 0;
	for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
	{
		if (l_staLCPConfReqItem[i].blIsNegoRequired && l_staLCPConfReqItem[i].pfunFillReqItem)		
			unWriteIdx += l_staLCPConfReqItem[i].pfunFillReqItem(&ubaConfReq[unWriteIdx]); 		
	}	
	fprintf(stderr, "]\r\n");
	return LNCPSendPacket(hPPP, PPP_LCP, EN_CPCODE::CONFREQ, ubIdentifire, ubaConfReq, unWriteIdx, TRUE);
}

void SendTerminateReq(HANDLE& hPPP)
{
	UCHAR ubIdentifire = l_atubLCPIdentifier++;
	fprintf(stderr, "sent [Protocol LCP, Id = %02X, Code = 'Terminate Request']\r\n", ubIdentifire);	
	LNCPSendPacket(hPPP, PPP_LCP, EN_CPCODE::TERMREQ, ubIdentifire, (UCHAR *)"Neo Request", sizeof("Neo Request") - 1, TRUE);
}

BOOL IPCPConfReqAddr(HANDLE& hPPPDev)
{
	UCHAR ubIdentifire = l_atubIPCPIdentifier++;
	fprintf(stderr, "sent [Protocol IPCP, Id = %02X, Code = 'Configure Request'", ubIdentifire);
	UCHAR ubaConfReq[128];
	UINT unWriteIdx = 0;
	for (INT i = 0; i < IPCP_CONFREQ_NUM; i++)
	{
		if (l_staIPCPConfReqItem[i].blIsNegoRequired && l_staIPCPConfReqItem[i].pfunFillReqItem)
			unWriteIdx += l_staIPCPConfReqItem[i].pfunFillReqItem(&ubaConfReq[unWriteIdx]);
	}
	fprintf(stderr, "]\r\n");

	if (EN_IPCPNEGOSTATE::RCVEDNAK == l_enIPCPNegoState)
		l_enIPCPNegoState = EN_IPCPNEGOSTATE::CONFIRMADDR;
	return LNCPSendPacket(hPPPDev, PPP_IPCP, EN_CPCODE::CONFREQ, ubIdentifire, ubaConfReq, unWriteIdx, TRUE);
}

INT ReadPacket(HANDLE& hPPP, HANDLE& hPPPDev, fd_set fdsIn, HANDLE& hMaxIn, UCHAR *pubReadBuf, INT nReadBufLen)
{
	struct timeval stTimeout;
	INT nRtnVal;
#if DEBUG_LEVEL
	HANDLE hRead;
#endif
	INT nRcvBytes; 

	stTimeout.tv_sec  = 1;
	stTimeout.tv_usec = 0;
	nRtnVal = select(hMaxIn + 1, &fdsIn, NULL, NULL, &stTimeout);
	if (nRtnVal > 0)
	{
		//* 读取到达的报文
		if (FD_ISSET(hPPP, &fdsIn))
		{
		#if DEBUG_LEVEL
			hRead = hPPP;
		#endif
			nRcvBytes = read(hPPP, pubReadBuf, nReadBufLen);
			if (nRcvBytes > 0)
				goto __lblRtnPacket;
		}
		else if (FD_ISSET(hPPPDev, &fdsIn))
		{
		#if DEBUG_LEVEL
			hRead = hPPPDev;
		#endif
			nRcvBytes = read(hPPPDev, pubReadBuf, nReadBufLen);
			if (nRcvBytes > 0)
				goto __lblRtnPacket;
		}
		else;
	}

	return 0;
	
__lblRtnPacket: 
#if DEBUG_LEVEL
	fprintf(stderr, "<%d>recv %d bytes: ", hRead, nRcvBytes);
	PrintHexArrayByStderr(pubReadBuf, nRcvBytes, 32);
#endif

	return nRcvBytes;
}

static BOOL LCPNegoHandler_CONFREQ(HANDLE& hPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket;

	INT nReadIdx = sizeof(ST_LNCP_ACFC_HDR);
	while (nReadIdx < nPacketLen)
	{
		for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == l_staLCPConfReqItem[i].ubType)
				if (l_staLCPConfReqItem[i].pfunGetReqVal)
					nReadIdx += l_staLCPConfReqItem[i].pfunGetReqVal(pubPacket + nReadIdx, nullptr);
		}
	}
	fprintf(stderr, "]\r\n");

	//fprintf(stderr, "MRU: %d, ACCM: %08X, Auth Type: %04X %02X, Protocol Comp: %d, Addr Ctl Comp: %d\r\n",
	//		l_stNegoResult.stLCP.usMRU, l_stNegoResult.stLCP.unACCM, l_stNegoResult.stLCP.stAuth.usType,
	//		l_stNegoResult.stLCP.stAuth.ubaData[0], l_stNegoResult.stLCP.blIsProtoComp, l_stNegoResult.stLCP.blIsAddrCtlComp); 
	fprintf(stderr, "sent [Protocol LCP, Id = %02X, Code = 'Configure Ack']\r\n", pstHdr->ubIdentifier);
	return LNCPSendPacket(hPPP, PPP_LCP, (UCHAR)EN_CPCODE::CONFACK, pstHdr->ubIdentifier, pubPacket + sizeof(ST_LNCP_ACFC_HDR), (size_t)nPacketLen - sizeof(ST_LNCP_ACFC_HDR), FALSE); 
}

static BOOL LCPNegoHandler_CONFACK(HANDLE& hPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket;
	
	USHORT usProtocol = ntohs(pstHdr->usProtocol);
	l_pobjLockWALinkedList->lock();
	{
		PST_WA_LINKEDLISTNODE pstNode = FindWANode(usProtocol, pstHdr->ubIdentifier);
		if (pstNode)
		{
			//FreeNodeOfWALinkedList(pstNode);	//* 这里不能释放节点只能由定时器释放才可避免收到应答时等待报文亦同时超时的问题
			pstNode->blIsAcked = TRUE;			//* 在这里我们只是打上已收到应答的标志即可，确保超时到达时不触发链路回归事件
			pstNode->pobjTimer->recount(1);		//* 1秒后立即删除该节点，因为已经收到应答了，不用傻傻的等待定时器超时了
		}
	}	
	l_pobjLockWALinkedList->unlock();
	fprintf(stderr, "]\r\n");

	fprintf(stderr, "use %s authentication\r\n", GetProtocolName(l_stNegoResult.stLCP.stAuth.usType).c_str());

	//fprintf(stderr, "recved ack <Protocol %s, Code = '%s', Id = %d>\r\n", GetProtocolName(usProtocol).c_str(), GetCPCodeName((EN_CPCODE)pstHdr->ubCode).c_str(), pstHdr->ubIdentifier);

	return TRUE;
}

static BOOL LCPNegoHandler_TERMACK(HANDLE& hPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket;

	USHORT usProtocol = ntohs(pstHdr->usProtocol);
	l_pobjLockWALinkedList->lock();
	{
		PST_WA_LINKEDLISTNODE pstNode = FindWANode(usProtocol, pstHdr->ubIdentifier);
		if (pstNode)
		{
			//FreeNodeOfWALinkedList(pstNode);	//* 这里不能释放节点只能由定时器释放才可避免收到应答时等待报文亦同时超时的问题
			pstNode->blIsAcked = TRUE;			//* 在这里我们只是打上已收到应答的标志即可，确保超时到达时不触发链路回归事件
			pstNode->pobjTimer->recount(1);		//* 1秒后立即删除该节点，因为已经收到应答了，不用傻傻的等待定时器超时了
		}
	}
	l_pobjLockWALinkedList->unlock();
	fprintf(stderr, "]\r\nLink terminated.\r\nppp0 <-/-> %s\r\n", PPP_DEV_PORT);

	return FALSE;
}

static BOOL IPCPNegoHandler_CONFREQ(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket;

	INT nReadIdx = sizeof(ST_LNCP_ACFC_HDR);
	while (nReadIdx < nPacketLen)
	{
		for (INT i = 0; i < IPCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == l_staIPCPConfReqItem[i].ubType)
				if (l_staIPCPConfReqItem[i].pfunGetReqVal)
					nReadIdx += l_staIPCPConfReqItem[i].pfunGetReqVal(pubPacket + nReadIdx, nullptr);
		}
	}
	fprintf(stderr, "]\r\n");

	fprintf(stderr, "sent [Protocol IPCP, Id = %02X, Code = 'Configure ACK']\r\n", pstHdr->ubIdentifier);
	return LNCPSendPacket(hPPPDev, PPP_IPCP, (UCHAR)EN_CPCODE::CONFACK, pstHdr->ubIdentifier, pubPacket + sizeof(ST_LNCP_ACFC_HDR), (size_t)nPacketLen - sizeof(ST_LNCP_ACFC_HDR), FALSE);
}

static BOOL IPCPNegoHandler_CONFACK(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket;

	USHORT usProtocol = ntohs(pstHdr->usProtocol);
	l_pobjLockWALinkedList->lock();
	{
		PST_WA_LINKEDLISTNODE pstNode = FindWANode(usProtocol, pstHdr->ubIdentifier);
		if (pstNode)
		{
			//FreeNodeOfWALinkedList(pstNode);	//* 这里不能释放节点只能由定时器释放才可避免收到应答时等待报文亦同时超时的问题
			pstNode->blIsAcked = TRUE;			//* 在这里我们只是打上已收到应答的标志即可，确保超时到达时不触发链路回归事件
			pstNode->pobjTimer->recount(1);		//* 1秒后立即删除该节点，因为已经收到应答了，不用傻傻的等待定时器超时了
		}
	}
	l_pobjLockWALinkedList->unlock();
	fprintf(stderr, "]\r\n");

	if (EN_IPCPNEGOSTATE::CONFIRMADDR == l_enIPCPNegoState)
	{
		fprintf(stderr, "IPCP negotiation succeeded. \r\n");
		
		struct in_addr stAddr = { l_stNegoResult.stIPCP.unAddr };
		fprintf(stderr, "    Local IP Address %s\r\n", inet_ntoa(stAddr));
		stAddr = { l_stNegoResult.stIPCP.unPrimaryDNSAddr };
		fprintf(stderr, "  Primary DNS Server %s\r\n", inet_ntoa(stAddr));
		stAddr = { l_stNegoResult.stIPCP.unSecondaryDNSAddr };
		fprintf(stderr, "Secondary DNS Server %s\r\n", inet_ntoa(stAddr));

		((UCHAR *)&l_stNegoResult.stIPCP.unPointToPointAddr)[0] = ((UCHAR *)&l_stNegoResult.stIPCP.unAddr)[0]; 
		((UCHAR *)&l_stNegoResult.stIPCP.unPointToPointAddr)[1] = 42; 
		((UCHAR *)&l_stNegoResult.stIPCP.unPointToPointAddr)[2] = 42;
		((UCHAR *)&l_stNegoResult.stIPCP.unPointToPointAddr)[3] = 42;		

		l_enIPCPNegoState = EN_IPCPNEGOSTATE::FINISHED;
	}

	//fprintf(stderr, "recved ack <Protocol %s, Code = '%s', Id = %d>\r\n", GetProtocolName(usProtocol).c_str(), GetCPCodeName((EN_CPCODE)pstHdr->ubCode).c_str(), pstHdr->ubIdentifier);

	return TRUE;
}

static BOOL IPCPNegoHandler_CONFNAK(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen)
{
	INT nReadIdx = sizeof(ST_LNCP_ACFC_HDR);
	while (nReadIdx < nPacketLen)
	{
		for (INT i = 0; i < IPCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == l_staIPCPConfReqItem[i].ubType)
				if (l_staIPCPConfReqItem[i].pfunGetReqVal)
					nReadIdx += l_staIPCPConfReqItem[i].pfunGetReqVal(pubPacket + nReadIdx, nullptr);
		}
	}
	fprintf(stderr, "]\r\n");

	//* 收到应答，清除等待队列里的报文
#if 1
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket;
	USHORT usProtocol = ntohs(pstHdr->usProtocol);
	l_pobjLockWALinkedList->lock();
	{
		PST_WA_LINKEDLISTNODE pstNode = FindWANode(usProtocol, pstHdr->ubIdentifier);
		if (pstNode)
		{
			//FreeNodeOfWALinkedList(pstNode);	//* 这里不能释放节点只能由定时器释放才可避免收到应答时等待报文亦同时超时的问题
			pstNode->blIsAcked = TRUE;			//* 在这里我们只是打上已收到应答的标志即可，确保超时到达时不触发链路回归事件
			pstNode->pobjTimer->recount(1);		//* 1秒后立即删除该节点，因为已经收到应答了，不用傻傻的等待定时器超时了
		}
	}
	l_pobjLockWALinkedList->unlock();
#endif

	l_enIPCPNegoState = EN_IPCPNEGOSTATE::RCVEDNAK;
	return IPCPConfReqAddr(hPPPDev);
}

static BOOL IPCPNegoHandler_CONFREJ(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen)
{
	fprintf(stderr, "]\r\n");
	return TRUE; 
}

static BOOL IPCPNegoHandler_CODEREJ(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen)
{
	fprintf(stderr, "]\r\n");
	return TRUE;
}

BOOL LCPNegotiation(HANDLE& hPPP, UCHAR *pubPacket, INT nPacketLen) //* LCP/CHAP/PAP使用hPPP，NCP使用hPPPDev
{
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket;

	for (INT i = 0; i < CPCODE_NUM; i++)
	{ 
		if (l_staLCPNegoHandler[i].enCode == (EN_CPCODE)pstHdr->ubCode)
			if (l_staLCPNegoHandler[i].pfunHandler)
				return l_staLCPNegoHandler[i].pfunHandler(hPPP, pubPacket, nPacketLen);
	}
	fprintf(stderr, "]\r\n");

	return TRUE; 
}

BOOL CHAPSendResponse(HANDLE& hPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket;
	UCHAR ubResponse[sizeof(ST_CHAP_DATA) + 128]; //* 名称不会超过128字节，即使密码很长
	UCHAR ubaOriginal[sizeof(ST_CHAP_DATA) + 128]; 
	UINT unOriginalLen; 

	//* 使用MD5算法生成Challenge值：ubIdentifier + 拨号脚本设置的密码 + 对端下发的Challenge值
	PST_CHAP_DATA pstData = (PST_CHAP_DATA)(pubPacket + sizeof(ST_LNCP_ACFC_HDR));
	CHAR szChallenge[128] = { 0 };
	SprintfHexArray(pstData->ubaChallenge, (USHORT)pstData->ubChallengeLen, szChallenge, sizeof(szChallenge), FALSE);
	*((CHAR *)pubPacket + nPacketLen) = 0;
	fprintf(stderr, ", Challenge = <%s>, name = \"%s\"]\r\n", szChallenge, ((CHAR *)pstData) + sizeof(ST_CHAP_DATA));
	ubaOriginal[0] = pstHdr->ubIdentifier;
	unOriginalLen = 1;
	memcpy(&ubaOriginal[1], AUTH_PASSWORD, sizeof(AUTH_PASSWORD) - 1);
	unOriginalLen += (UINT)sizeof(AUTH_PASSWORD) - 1;
	memcpy(&ubaOriginal[unOriginalLen], pstData->ubaChallenge, pstData->ubChallengeLen);
	unOriginalLen += (UINT)pstData->ubChallengeLen;
	//SprintfHexArray(ubaOriginal, unOriginalLen, szChallenge, sizeof(szChallenge));
	//fprintf(stderr, "Original: %s\r\n", szChallenge);
	ST_MD5VAL stChallengeCode = MD5Cal(ubaOriginal, unOriginalLen);
	ubResponse[0] = CHAP_CHALLENGE_LEN; 
	memcpy(&ubResponse[1], (UCHAR *)&stChallengeCode, sizeof(stChallengeCode));
	memcpy(&ubResponse[1 + CHAP_CHALLENGE_LEN], AUTH_USER, sizeof(AUTH_USER) - 1);

	SprintfHexArray(&ubResponse[1], CHAP_CHALLENGE_LEN, szChallenge, sizeof(szChallenge), FALSE);
	fprintf(stderr, "sent [Protocol CHAP, Id = %02X, Code = 'Response', Challenge = <%s>, name = \"%s\"]\r\n", pstHdr->ubIdentifier, szChallenge, AUTH_USER);
	return LNCPSendPacket(hPPP, PPP_CHAP, EN_CHAPCODE::RESPONSE, pstHdr->ubIdentifier, ubResponse, 1 + CHAP_CHALLENGE_LEN + sizeof(AUTH_USER) - 1, TRUE);
}

BOOL CHAPSuccess(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket;
	USHORT usProtocol = ntohs(pstHdr->usProtocol);
	l_pobjLockWALinkedList->lock();
	{
		PST_WA_LINKEDLISTNODE pstNode = FindWANode(usProtocol, pstHdr->ubIdentifier);
		if (pstNode)
		{
			//FreeNodeOfWALinkedList(pstNode);	//* 这里不能释放节点只能由定时器释放才可避免收到应答时等待报文亦同时超时的问题
			pstNode->blIsAcked = TRUE;			//* 在这里我们只是打上已收到应答的标志即可，确保超时到达时不触发链路回归事件
			pstNode->pobjTimer->recount(1);		//* 1秒后立即删除该节点，因为已经收到应答了，不用傻傻的等待定时器超时了
		}
	}
	l_pobjLockWALinkedList->unlock();

	fprintf(stderr, "]\r\nCHAP authentication succeeded. \r\n");
	return IPCPConfReqAddr(hPPPDev);
}

BOOL CHAPAuthenticate(HANDLE& hPPP, HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket; 

	switch ((EN_CHAPCODE)pstHdr->ubCode)
	{
	case EN_CHAPCODE::CHALLENGE:
		if (l_stNegoResult.stLCP.stAuth.usType == PPP_CHAP)
		{
			if (l_stNegoResult.stLCP.stAuth.ubaData[0] == 0x05)			
				return CHAPSendResponse(hPPP, pubPacket, nPacketLen); 			
			else
			{
				fprintf(stderr, "]\r\nCHAP认证算法固定为MD5，协商确定的算法类型[%02X]未知\r\n", l_stNegoResult.stLCP.stAuth.ubaData[0]);
				return FALSE;
			}
		}
		else
		{
			fprintf(stderr, "]\r\nLCP协商确定的认证协议为%s，收到的认证报文类型为CHAP，协商结果与实际认证协议不匹配\r\n", GetProtocolName(l_stNegoResult.stLCP.stAuth.usType).c_str());
			return FALSE;
		}
		
	case EN_CHAPCODE::SUCCESS:
		return CHAPSuccess(hPPPDev, pubPacket, nPacketLen); 

	case EN_CHAPCODE::FAILURE:
		fprintf(stderr, "]\r\n协商失败，请联系Neo解决此问题\r\n");
		return FALSE;

	case EN_CHAPCODE::RESPONSE:
		fprintf(stderr, "]\r\n协商流程错误，接收端不应该收到挑战应答报文，请联系Neo解决此问题\r\n");
		return FALSE;
	}

	fprintf(stderr, "]\r\n收到的CHAP协议代码域值[%02X]不在系统支持的范围之内\r\n", pstHdr->ubCode);

	return FALSE; 
}

BOOL IPCPNegotiation(HANDLE& hPPPDev, UCHAR *pubPacket, INT nPacketLen) //* LCP/CHAP/PAP使用hPPP，NCP使用hPPPDev
{
	PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)pubPacket;

	for (INT i = 0; i < CPCODE_NUM; i++)
	{
		if (l_staIPCPNegoHandler[i].enCode == (EN_CPCODE)pstHdr->ubCode)
			if (l_staIPCPNegoHandler[i].pfunHandler)
				return l_staIPCPNegoHandler[i].pfunHandler(hPPPDev, pubPacket, nPacketLen);
	}
	fprintf(stderr, "]\r\n");

	return TRUE;
}

BOOL Negotiation(HANDLE& hPPP, HANDLE& hPPPDev, fd_set& fdsIn, HANDLE& hMaxIn)
{
	UCHAR ubaReadBuf[sizeof(ST_PPP_HDR) + PPP_MRU + sizeof(ST_PPP_TAIL)];
	INT nRcvBytes;
	static BOOL blIsFirstCallFinshed = FALSE;

__lblRead:
	if (l_blIsNeedToRecreatePPPLink)
		return FALSE;

	if (!blIsFirstCallFinshed)
	{
		if (EN_IPCPNEGOSTATE::FINISHED == l_enIPCPNegoState)
		{
			blIsFirstCallFinshed = TRUE;
			return TRUE;
		}

		if (!l_blIsRunning)					
			return TRUE;		
	}

	nRcvBytes = ReadPacket(hPPP, hPPPDev, fdsIn, hMaxIn, ubaReadBuf, sizeof(ubaReadBuf));
	if (nRcvBytes > 0) //* 内核ppp模块已经把收到的ppp帧分析、组装完整，这里只要读取到数据即意味着这是一个完整ppp报文，可以直接处理
	{
		PST_LNCP_ACFC_HDR pstHdr = (PST_LNCP_ACFC_HDR)ubaReadBuf;
		USHORT usProtocol = ntohs(pstHdr->usProtocol); 		
		fprintf(stderr, "recv [Protocol %s, Id = %02X, ", GetProtocolName(usProtocol).c_str(), pstHdr->ubIdentifier);
		switch (usProtocol)
		{
		case PPP_LCP:
			fprintf(stderr, "Code = '%s'",GetCPCodeName((EN_CPCODE)pstHdr->ubCode).c_str());
			if (!LCPNegotiation(hPPP, ubaReadBuf, nRcvBytes))
				return FALSE;
			break; 

		case PPP_CHAP:
			fprintf(stderr, "Code = '%s'", GetCHAPCodeName((EN_CHAPCODE)pstHdr->ubCode).c_str());
			if (!CHAPAuthenticate(hPPP, hPPPDev, ubaReadBuf, nRcvBytes))
				return FALSE; 
			break;

		case PPP_IPCP:
			fprintf(stderr, "Code = '%s'", GetCPCodeName((EN_CPCODE)pstHdr->ubCode).c_str());
			if (!IPCPNegotiation(hPPPDev, ubaReadBuf, nRcvBytes))
				return FALSE;
			break;

		default:
			break;
		}
	}

	goto __lblRead; 

	return TRUE; 
}

void UpdateDNS(void)
{
	FILE *pfResolv = fopen("/etc/resolv.conf", "w");
	if (!pfResolv)
	{
		fprintf(stderr, "/etc/resolv.conf文件打开失败, %s, 错误码: %d\r\n", strerror(errno), errno);
		return; 
	}
	
	struct in_addr stAddr = { l_stNegoResult.stIPCP.unPrimaryDNSAddr };
	fprintf(pfResolv, "nameserver %s\n", inet_ntoa(stAddr));
	stAddr = { l_stNegoResult.stIPCP.unSecondaryDNSAddr };
	fprintf(pfResolv, "nameserver %s\n", inet_ntoa(stAddr));

	if (ferror(pfResolv))
		fprintf(stderr, "/etc/resolv.conf文件写入失败,但这不影响IP地址直接访问, %s, 错误码: %d\r\n", strerror(errno), errno);
	fclose(pfResolv);
}

//* 内核层面设置ppp0的网络地址
static BOOL SetNetIfAddr(SOCKET& hSocket, string& strIfName)
{
	struct ifreq stIfr = { 0 };	

	//* 初始化
	stIfr.ifr_addr.sa_family    = AF_INET; 
	stIfr.ifr_dstaddr.sa_family = AF_INET; 
	stIfr.ifr_netmask.sa_family = AF_INET; 	
	sprintf(stIfr.ifr_name, "%s", strIfName.c_str());

	//* 设置网络接口地址
	((struct sockaddr_in*)&(stIfr.ifr_addr))->sin_addr.s_addr = l_stNegoResult.stIPCP.unAddr; 
	if (ioctl(hSocket, SIOCSIFADDR, (caddr_t)&stIfr) < 0 && errno != EIO)
	{
		fprintf(stderr, "网络接口<%s>的IP地址设置失败, %s, 错误码: %d\r\n", strIfName.c_str(), strerror(errno), errno);
		return FALSE;
	}

	//* 设置网关
	((struct sockaddr_in*)&(stIfr.ifr_dstaddr))->sin_addr.s_addr = l_stNegoResult.stIPCP.unPointToPointAddr;
	if (ioctl(hSocket, SIOCSIFDSTADDR, (caddr_t)&stIfr) < 0 && errno != EIO)
	{
		fprintf(stderr, "网络接口<%s>的网关地址设置失败, %s, 错误码: %d\r\n", strIfName.c_str(), strerror(errno), errno);
		return FALSE;
	}

	//* 设置子网掩码
	((struct sockaddr_in*)&(stIfr.ifr_netmask))->sin_addr.s_addr = l_stNegoResult.stIPCP.unNetMask;
	if (ioctl(hSocket, SIOCSIFNETMASK, (caddr_t)&stIfr) < 0 && errno != EIO)
	{
		fprintf(stderr, "网络接口<%s>的子网掩码设置失败, %s, 错误码: %d\r\n", strIfName.c_str(), strerror(errno), errno);
		return FALSE;
	}

	return TRUE;
}

//* 内核层面取消ppp0的网络地址
static BOOL UnsetNetIfAddr(SOCKET& hSocket, string& strIfName)
{
	struct ifreq stIfr = { 0 };

	stIfr.ifr_addr.sa_family = AF_INET;
	sprintf(stIfr.ifr_name, "%s", strIfName.c_str());
	if (ioctl(hSocket, SIOCSIFADDR, (caddr_t)&stIfr) < 0 && errno != EIO)
	{
		fprintf(stderr, "网络接口<%s>的IP地址清零操作失败, %s, 错误码: %d\r\n", strIfName.c_str(), strerror(errno), errno);
		return FALSE;
	}

	return TRUE;
}

//* 设置缺省路由
static BOOL SetDefaultRoute(SOCKET& hSocket, string& strIfName)
{
	//* 删除缺省路由条目（假如存在的话），为ppp链路成为缺省路由条目做准备
	system("route del default");
	
	struct rtentry stRte = { 0 };
	stRte.rt_dst.sa_family     = AF_INET;	
	stRte.rt_genmask.sa_family = AF_INET;
	((struct sockaddr_in*)&(stRte.rt_genmask))->sin_addr.s_addr = 0; //* 缺省路由条目的目标地址掩码为0，这样任意地址都将被允许
	stRte.rt_dev    = (CHAR *)strIfName.c_str();
	stRte.rt_metric = 0; //* 路由距离，到达指定网络所需的中转数
	stRte.rt_flags  = RTF_UP;
	if (ioctl(hSocket, SIOCADDRT, &stRte) < 0 && errno != EIO)
	{
		fprintf(stderr, "将网络接口<%s>添加到路由表失败, %s, 错误码: %d\r\n", strIfName.c_str(), strerror(errno), errno);
		return FALSE;
	}

	return TRUE;
}

//* 删除缺省路由
static BOOL UnsetDefaultRoute(SOCKET& hSocket, string& strIfName)
{
	struct rtentry stRte = { 0 };
	stRte.rt_dst.sa_family     = AF_INET;
	stRte.rt_gateway.sa_family = AF_INET;
	stRte.rt_genmask.sa_family = AF_INET;
	((struct sockaddr_in*)&(stRte.rt_genmask))->sin_addr.s_addr = 0; //* 缺省路由条目的目标地址掩码为0，这样任意地址都将被允许
	stRte.rt_dev = (CHAR *)strIfName.c_str();
	stRte.rt_metric = 0; //* 路由距离，到达指定网络所需的中转数
	stRte.rt_flags = RTF_UP;
	if (ioctl(hSocket, SIOCDELRT, &stRte) < 0 && errno != EIO && errno != ESRCH)
	{
		fprintf(stderr, "缺省路由条目删除失败, %s, 错误码: %d\r\n", strerror(errno), errno);
		return FALSE;
	}

	return TRUE;
}

//* 设置网卡状态：开启或关闭
static BOOL SetNetIfState(string& strIfName, SOCKET& hSocket, BOOL blIsIfUp)
{
	struct ifreq stIfr = { 0 };
	sprintf(stIfr.ifr_name, "%s", strIfName.c_str());

	if (ioctl(hSocket, SIOCGIFFLAGS, (caddr_t)&stIfr) < 0 && errno != EIO)
	{
		fprintf(stderr, "获取网络接口<%s>的当前状态标志数据失败, %s, 错误码: %d\r\n", strIfName.c_str(), strerror(errno), errno);
		return FALSE;
	}

	//* 启动或关闭接口
	if (blIsIfUp)
		stIfr.ifr_flags |= IFF_UP;
	else
		stIfr.ifr_flags &= ~IFF_UP;
	stIfr.ifr_flags |= IFF_POINTOPOINT;
	if (ioctl(hSocket, SIOCSIFFLAGS, (caddr_t)&stIfr) < 0 && errno != EIO)
	{
		fprintf(stderr, "修改网络接口<%s>的当前状态失败, %s, 错误码: %d\r\n", strIfName.c_str(), strerror(errno), errno);
		return FALSE;
	}

	return TRUE; 
}

//* 设置网络协议模式
static BOOL SetNetworkPacketMode(HANDLE& hPPPDev, USHORT usProtocol, NPmode enMode)
{
	struct npioctl stNPMode;

	stNPMode.protocol = usProtocol;
	stNPMode.mode     = enMode;
	if (ioctl(hPPPDev, PPPIOCSNPMODE, (caddr_t)&stNPMode) < 0 && errno != EIO)
	{
		fprintf(stderr, "修改ppp通道允许通过的网络报文模式失败, %s, 错误码: %d\r\n", strerror(errno), errno);
		return FALSE;
	}
	return TRUE;
}

static BOOL NetIfUp(HANDLE& hPPPDev, string& strIfName)
{
	BOOL blRtnVal = FALSE;

	SOCKET hSocket;
	if ((hSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		fprintf(stderr, "socket()函数执行失败, %s, 错误码: %d\r\n", strerror(errno), errno);
		return FALSE;
	}

	do {		
		UpdateDNS();

		if (!SetNetIfAddr(hSocket, strIfName))
			break;		

		if (!SetNetIfState(strIfName, hSocket, TRUE))
			break; 

		if (!SetDefaultRoute(hSocket, strIfName))
			break;
		
		if (!SetNetworkPacketMode(hPPPDev, PPP_IP, NPmode::NPMODE_PASS))
			break; 

		blRtnVal = TRUE;
	} while (FALSE);	

	close(hSocket); 
	return blRtnVal;
}

//* 退出ppp模式
void ExitPPPMode(SerialPort& objSerialPort)
{
	HANDLE& hPort = objSerialPort.GetHandle();

	//* 清空该串口收发缓冲区中的原有数据，同时确保下面恢复线路规程时不被挂起
	tcflush(hPort, TCIOFLUSH);

	//* 恢复线路规程
	INT nLineDiscip = N_TTY;
	if (ioctl(hPort, TIOCSETD, &nLineDiscip) < 0 && errno != EIO)
		fprintf(stderr, "恢复modem口线路规程为普通tty类型失败, %s, 错误码: %d\r\n", strerror(errno), errno);	

	if (ioctl(hPort, TIOCNXCL, 0) < 0 && errno != EIO)  //* 取消tty终端为串行线路专用模式	
		fprintf(stderr, "取消modem口为串行线路专用模式失败, %s, 错误码: %d\r\n", strerror(errno), errno); 

	objSerialPort.dtr(0);
	Sleep(1); //* 其实完全没必要，只是为了增加一丝丝”顿挫“感
}

static void LCPDown(SerialPort& objSerialPort, HANDLE& hPPP, HANDLE& hPPPDev, string& strIfName, fd_set& fdsIn, HANDLE& hMaxIn, string& strPIDFile)
{
	SetNetworkPacketMode(hPPPDev, PPP_IP, NPMODE_DROP);

	SOCKET hSocket;
	if ((hSocket = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
		SetNetIfState(strIfName, hSocket, FALSE);
	else
		fprintf(stderr, "socket()函数执行失败, %s, 错误码: %d\r\n", strerror(errno), errno);		

	UnsetDefaultRoute(hSocket, strIfName); 
	UnsetNetIfAddr(hSocket, strIfName); 

	if (!(hSocket < 0))
		close(hSocket); 

	SendTerminateReq(hPPP); 
	Negotiation(hPPP, hPPPDev, fdsIn, hMaxIn);	
	unlink(strPIDFile.c_str());	
	ExitPPPMode(objSerialPort);
}

//* 数据库，表设计->选项->行格式->COMPACT会减少空间占用
INT main(INT argc, CHAR *argv[])
{
	BOOL blIsResetModem = TRUE;

	InterceptExitSignal(SYSClose);

	//* 判断linux系统是否编译进了ppp模块
	INT nDummyFD = open("/dev/ppp", O_RDWR);
	if (nDummyFD < 0)
	{
		cout << "Couldn't open the /dev/ppp device, ";
		if (errno == ENOENT)
			cout << "You need to create the /dev/ppp device node by executing the following command as root: "
			<< endl << "	mknod /dev/ppp c 108 0" << endl;
		else if (errno == ENODEV || errno == ENXIO)
			cout << "Please load the ppp_generic kernel module." << endl;
		else
			cout << strerror(errno) << endl;
		return -1;
	}
	close(nDummyFD); 

	SerialPort objSerialPort;
	string strPort(PPP_DEV_PORT);
	SerialPort::ST_SERIAL_PORT stSerialPort { 115200, 'N', 8, 1, SerialPort::EN_FLOWCONTROL::FL_NOCRTSCTS };
	if (!objSerialPort.init(strPort, stSerialPort))
	{
		cout <<  "Failed to open serial port [" << strPort << "], " << objSerialPort.GetLastError() << endl;
		return -1; 
	}
	else
	{
		cout << "Serial port [" << strPort << "] open succeeded." << endl;

		//* 防止modem挂机
		objSerialPort.dtr(0);
		Sleep(1);
		objSerialPort.dtr(1);
	}	

	//* 获得串口句柄
	HANDLE& hPort = objSerialPort.GetHandle();
	INT nStdInFD  = dup(STDIN_FILENO);	
	INT nStdOutFD = dup(STDOUT_FILENO);	
	dup2(hPort, STDIN_FILENO);
	dup2(hPort, STDOUT_FILENO);
	{
		BOOL blIsFault = FALSE;
		EN_LINK_STATE enState = MODEMRDY; //* 必须先确保modem就绪

		INT nChannelIdx;
		HANDLE hPPP = INVALID_HANDLE, hPPPDev = INVALID_HANDLE;
		INT nIfUnit;
		string strIfName;
		string strPIDFile;
		fd_set fdsIn;
		HANDLE hMaxIn = 0;
		while (l_blIsRunning && !blIsFault && !l_blIsNeedToRecreatePPPLink)
		{
			switch (enState)
			{
			case MODEMRDY: 
				if (chat())
					enState = LOWERUP; //* 这里不再break，直接进入下一个阶段LOWERUP，而不是循环一圈后再开启LOWERUP阶段
				else
				{
					blIsFault = TRUE;
					break;
				}

			case LOWERUP: //* 底层就绪，其实就是配置内核ppp模块确保其做好ppp联网的准备
				if (EnterPPPMode(hPort, nChannelIdx, hPPP, hPPPDev, nIfUnit, fdsIn, hMaxIn, strIfName, strPIDFile))
				{
					enState = STARTNEGOTIATION; 
					Sleep(1); //* 状态机在进入下一个阶段前休眠一小段时间以确保底层环境真正就绪，然后直接进入下一阶段（不循环一圈后再进入）
				}
				else
				{
					blIsFault = TRUE;
					break;
				}

			case STARTNEGOTIATION:
				if (StartNegotiation(hPPP))
				{
					enState = NEGOTIATION;
				}
				else
				{
					blIsFault = TRUE;
					break;
				}

			case NEGOTIATION:				
				if(Negotiation(hPPP, hPPPDev, fdsIn, hMaxIn))
					enState = IFUP;
				else
				{
					blIsFault = TRUE;
					break;
				}

			case IFUP:
				fprintf(stderr, "Starting %s ...\r\n", strIfName.c_str());
				if (NetIfUp(hPPPDev, strIfName))
				{
					fprintf(stderr, "%s started\r\n", strIfName.c_str());
					enState = ESTABLISHED;
				}
				else
				{
					blIsFault = TRUE;
					break;
				}

			case ESTABLISHED:
				Sleep(1);
				break;
			}				
		}

		//* 如果已经建立链路则先关闭该链路
		if (ESTABLISHED == enState)
		{
			LCPDown(objSerialPort, hPPP, hPPPDev, strIfName, fdsIn, hMaxIn, strPIDFile);
			blIsResetModem = FALSE;
		}

		//* 回收相关资源
		if (hPPP != INVALID_HANDLE)
			close(hPPP);
		if (hPPPDev != INVALID_HANDLE)
			close(hPPPDev);
		if (l_pobjLockWALinkedList)
			delete l_pobjLockWALinkedList; 
	}	
	dup2(nStdInFD, STDIN_FILENO);		//* 恢复标准输出
	dup2(nStdOutFD, STDOUT_FILENO);

	close(nStdInFD);
	close(nStdOutFD); 	
				
	if (blIsResetModem)
	{
		printf("resetting modem\r\n"); //* 前面恢复标准输出后，如果使用cout还是无法正常显示调试信息，只能使用printf才可，说明cout的输出机制与printf的输出机制有区别
		system("4g_poweroff");
		Sleep(3);
		system("4g_poweron");
	}	

	printf("system exit. \r\n");

	return 0;
}
#include "port/datatype.h"
#include "errors.h"
#include "utils.h"
#include "md5.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"

#if SUPPORT_PPP
#include "ppp/negotiation.h""
#define SYMBOL_GLOBALS
#include "ppp/ipcp.h"
#undef SYMBOL_GLOBALS
#include "ppp/ppp.h"

//* IPCP配置请求处理函数
static INT put_addr(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult);
static INT put_primary_dns(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult);
static INT put_secondary_dns(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult);
static INT get_addr(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult);
static INT get_primary_dns(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult);
static INT get_secondary_dns(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult);
static const ST_LNCP_CONFREQ_ITEM lr_staConfReqItem[IPCP_CONFREQ_NUM] =
{
	{ (UCHAR)ADDRS,				   "Addresses Option",		FALSE, NULL, NULL },
	{ (UCHAR)COMPRESSION_PROTOCOL, "Compression protocol",	FALSE, NULL, NULL },
	{ (UCHAR)ADDR,				   "IP address",			TRUE,  put_addr,		  get_addr },
	{ (UCHAR)PRIMARYDNS,		   "Primary DNS address",	TRUE,  put_primary_dns,   get_primary_dns },
	{ (UCHAR)SECONDARYDNS,		   "Secondary DNS address", TRUE,  put_secondary_dns, get_secondary_dns },
};

//* IPCP协商处理函数
#define IPCPCODE_NUM	5	//* 协商阶段代码域类型数量
static BOOL conf_request(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static BOOL conf_ack(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static BOOL conf_nak(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static BOOL conf_reject(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static BOOL code_reject(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static const ST_LNCPNEGOHANDLER lr_staNegoHandler[IPCPCODE_NUM] =
{
	{ CONFREQ, conf_request },
	{ CONFACK, conf_ack },
	{ CONFNAK, conf_nak },
	{ CONFREJ, conf_reject },
	{ CODEREJ, code_reject }
};

BOOL ipcp_send_conf_request(PSTCB_NETIFPPP pstcbPPP, EN_ERROR_CODE *penErrCode)
{
	UCHAR ubIdentifier = pstcbPPP->pstNegoResult->ubIdentifier++;
#if SUPPORT_PRINTF
	printf("sent [Protocol IPCP, Id = %02X, Code = 'Configure Request'", ubIdentifier);
#endif

	UCHAR ubaPacket[80];
	USHORT usWriteIdx = (USHORT)sizeof(ST_LNCP_HDR);
	INT i; 
	for (i = 0; i < IPCP_CONFREQ_NUM; i++)
	{
		if (lr_staConfReqItem[i].blIsNegoRequired && lr_staConfReqItem[i].pfunPut)
			usWriteIdx += lr_staConfReqItem[i].pfunPut(&ubaPacket[usWriteIdx], pstcbPPP->pstNegoResult);
	}
	printf("]\r\n");

	return send_nego_packet(pstcbPPP, PPP_IPCP, (UCHAR)CONFREQ, ubIdentifier, ubaPacket, usWriteIdx, TRUE, penErrCode);
}

//* pap协议接收函数
void ipcp_recv(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

#if SUPPORT_PRINTF
	printf("recv [Protocol IPCP, Id = %02X, Code = '%s'", pstHdr->ubIdentifier, get_cpcode_name((EN_CPCODE)pstHdr->ubCode));
#endif

	for (INT i = 0; i < CPCODE_NUM; i++)
	{
		if (lr_staNegoHandler[i].enCode == (EN_CPCODE)pstHdr->ubCode)
			if (lr_staNegoHandler[i].pfunHandler)
				return lr_staNegoHandler[i].pfunHandler(pstcbPPP, pubPacket, nPacketLen);
	}
#if SUPPORT_PRINTF
	printf("]\r\n");
#endif
}

#endif
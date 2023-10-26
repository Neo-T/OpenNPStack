/*
 * 遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "onps_errors.h"
#include "onps_utils.h"
#include "onps_md5.h"
#include "mmu/buf_list.h"

#if SUPPORT_PPP
#include "ppp/negotiation.h"
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
static void conf_request(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void conf_ack(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void conf_nak(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void conf_reject(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void code_reject(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static const ST_LNCPNEGOHANDLER lr_staNegoHandler[IPCPCODE_NUM] =
{
	{ CONFREQ, conf_request },
	{ CONFACK, conf_ack },
	{ CONFNAK, conf_nak },
	{ CONFREJ, conf_reject },
	{ CODEREJ, code_reject }
};

static INT put_addr(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult)
{
	PST_IPCP_CONFREQ_ADDR pstReq = (PST_IPCP_CONFREQ_ADDR)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)ADDR;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + 4;

	//UINT unAddr = htonl(l_stNegoResult.stIPCP.unAddr);	
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    UCHAR *pubAddr = (UCHAR *)&pstNegoResult->stIPCP.unAddr;
	printf(", IP <%d.%d.%d.%d>", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
#endif
	memcpy((UCHAR *)&pstReq->unVal, (UCHAR *)&pstNegoResult->stIPCP.unAddr, 4);
	return (INT)pstReq->stHdr.ubLen;
}

static INT put_primary_dns(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult)
{
	PST_IPCP_CONFREQ_ADDR pstReq = (PST_IPCP_CONFREQ_ADDR)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)PRIMARYDNS;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + 4;

	//UINT unAddr = htonl(l_stNegoResult.stIPCP.unPrimaryDNS);	
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    UCHAR *pubAddr = (UCHAR *)&pstNegoResult->stIPCP.unPrimaryDNS;
	printf(", Primary DNS <%d.%d.%d.%d>", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
#endif
	memcpy((UCHAR *)&pstReq->unVal, (UCHAR *)&pstNegoResult->stIPCP.unPrimaryDNS, 4);
	return (INT)pstReq->stHdr.ubLen;
}

static INT put_secondary_dns(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult)
{
	PST_IPCP_CONFREQ_ADDR pstReq = (PST_IPCP_CONFREQ_ADDR)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)SECONDARYDNS;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + 4;

	//UINT unAddr = htonl(l_stNegoResult.stIPCP.unSecondaryDNSAddr);	
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    UCHAR *pubAddr = (UCHAR *)&pstNegoResult->stIPCP.unSecondaryDNS;
	printf(", Secondary DNS <%d.%d.%d.%d>", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
#endif
	memcpy((UCHAR *)&pstReq->unVal, (UCHAR *)&pstNegoResult->stIPCP.unSecondaryDNS, 4);
	return (INT)pstReq->stHdr.ubLen;
}

static INT get_addr(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult)
{
	PST_IPCP_CONFREQ_ADDR pstItem = (PST_IPCP_CONFREQ_ADDR)pubItem;
	//UINT unVal = ntohl(pstItem->unVal);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&pstItem->unVal, sizeof(pstItem->unVal));
	pstNegoResult->stIPCP.unAddr = pstItem->unVal;
	
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    UCHAR *pubAddr = (UCHAR *)&pstItem->unVal;
	printf(", IP <%d.%d.%d.%d>", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
#endif

	return (INT)pstItem->stHdr.ubLen;
}

static INT get_primary_dns(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult)
{
	PST_IPCP_CONFREQ_ADDR pstItem = (PST_IPCP_CONFREQ_ADDR)pubItem;
	//UINT unVal = ntohl(pstItem->unVal);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&pstItem->unVal, sizeof(pstItem->unVal));
	pstNegoResult->stIPCP.unPrimaryDNS = pstItem->unVal;
		
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    UCHAR *pubAddr = (UCHAR *)&pstItem->unVal;
	printf(", Primary DNS <%d.%d.%d.%d>", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
#endif

	return (INT)pstItem->stHdr.ubLen;
}

static INT get_secondary_dns(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult)
{
	PST_IPCP_CONFREQ_ADDR pstItem = (PST_IPCP_CONFREQ_ADDR)pubItem;
	//UINT unVal = ntohl(pstItem->unVal);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&pstItem->unVal, sizeof(pstItem->unVal));
	pstNegoResult->stIPCP.unSecondaryDNS = pstItem->unVal;
		
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    UCHAR *pubAddr = (UCHAR *)&pstItem->unVal;
	printf(", Secondary DNS <%d.%d.%d.%d>", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
#endif

	return (INT)pstItem->stHdr.ubLen;
}

static void conf_request(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket; 

	BOOL blIsFound = TRUE;
	INT nReadIdx = sizeof(ST_LNCP_HDR);
	while (nReadIdx < nPacketLen && blIsFound)
	{
		//* 理论上如果出现ppp栈不支持的请求配置项应该是当前的ppp栈实现远远落后于ppp标准的发展，此时必须确保循环能够安全退出，当然，实际情况是应该每次for循环都能够找到对应的处理函数
		blIsFound = FALSE;
		for (INT i = 0; i < IPCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == lr_staConfReqItem[i].ubType)
			{
				if (lr_staConfReqItem[i].pfunGet)
					nReadIdx += lr_staConfReqItem[i].pfunGet(pubPacket + nReadIdx, NULL, pstcbPPP->pstNegoResult);
				blIsFound = TRUE;
			}
		}
	}
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	if(blIsFound)
		printf("]sent [Protocol IPCP, Id = %02X, Code = 'Configure ACK']\r\n", pstHdr->ubIdentifier);
	else
		printf(", code <%02X> is not supported]\r\nsent [Protocol IPCP, Id = %02X, Code = 'Configure Ack']\r\n", pubPacket[nReadIdx], pstHdr->ubIdentifier);
#endif

	send_nego_packet(pstcbPPP, IPCP, (UCHAR)CONFACK, pstHdr->ubIdentifier, pubPacket, nReadIdx, FALSE, NULL);
}

static void conf_ack(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, IPCP, pstHdr->ubIdentifier);

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("]\r\n");
    
    UCHAR *pubAddr = (UCHAR *)&pstcbPPP->pstNegoResult->stIPCP.unAddr;
	printf("    Local IP Address %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    pubAddr = (UCHAR *)&pstcbPPP->pstNegoResult->stIPCP.unPrimaryDNS;
	printf("  Primary DNS Server %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    pubAddr = (UCHAR *)&pstcbPPP->pstNegoResult->stIPCP.unSecondaryDNS;
	printf("Secondary DNS Server %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
#endif

	((UCHAR *)&pstcbPPP->pstNegoResult->stIPCP.unPointToPointAddr)[0] = ((UCHAR *)&pstcbPPP->pstNegoResult->stIPCP.unAddr)[0];
	((UCHAR *)&pstcbPPP->pstNegoResult->stIPCP.unPointToPointAddr)[1] = 42;
	((UCHAR *)&pstcbPPP->pstNegoResult->stIPCP.unPointToPointAddr)[2] = 42;
	((UCHAR *)&pstcbPPP->pstNegoResult->stIPCP.unPointToPointAddr)[3] = 42;

	//* 协商完毕，状态机可以迁移到ppp链路已成功建立的阶段了
	pstcbPPP->enState = ESTABLISHED; 
}

static void conf_nak(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, IPCP, pstHdr->ubIdentifier);

	BOOL blIsFound = TRUE;
	INT nReadIdx = sizeof(ST_LNCP_HDR);
	while (nReadIdx < nPacketLen && blIsFound)
	{
		blIsFound = FALSE;
		for (INT i = 0; i < IPCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == lr_staConfReqItem[i].ubType)
			{
				if (lr_staConfReqItem[i].pfunGet)				
					nReadIdx += lr_staConfReqItem[i].pfunGet(pubPacket + nReadIdx, NULL, pstcbPPP->pstNegoResult);					
				blIsFound = FALSE;
			}
		}
	}
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("]\r\n");
#endif

	EN_ONPSERR enErr; 
	if(ipcp_send_conf_request(pstcbPPP, &enErr))
		pstcbPPP->enState = WAITIPCPCONFACK;
	else
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL
		printf("ipcp_send_conf_request() failed, %s\r\n", onps_error(enErr));    
#endif
		pstcbPPP->enState = STACKFAULT;
	}
}

static void conf_reject(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, IPCP, pstHdr->ubIdentifier);

	//* 理论上这地方不可能出现死循环，因为这是对端反馈的“我方”请求配置的项，所以不应该不被“我方”识别
	INT nReadIdx = sizeof(ST_LNCP_HDR);
	while (nReadIdx < nPacketLen)
	{
		for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == lr_staConfReqItem[i].ubType)
			{
				nReadIdx += (INT)pubPacket[nReadIdx + 1];

            #if SUPPORT_PRINTF && DEBUG_LEVEL > 1
				printf(", %s", lr_staConfReqItem[i].pszName);
            #endif
			}
		}
	}

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("]\r\n, error: not all basic ip configuration requests succeed\r\n");
#endif

	pstcbPPP->enState = STACKFAULT;
}

static void code_reject(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;	

	//* 无论何种情形，收到应答都应该先清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, IPCP, pstHdr->ubIdentifier);

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    UCHAR ubRejCode = pubPacket[sizeof(ST_LNCP_HDR)];
	printf(", Reject Code = \"%s\", hex val = %02X]\r\nerror: ipcp code value not recognized by the peer.\r\n", get_cpcode_name((EN_CPCODE)ubRejCode), ubRejCode);
#endif

	pstcbPPP->enState = STACKFAULT;
}

BOOL ipcp_send_conf_request(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr)
{
	UCHAR ubIdentifier = pstcbPPP->pstNegoResult->ubIdentifier++;
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
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
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("]\r\n");
#endif

	return send_nego_packet(pstcbPPP, IPCP, (UCHAR)CONFREQ, ubIdentifier, ubaPacket, usWriteIdx, TRUE, penErr);
}

//* pap协议接收函数
void ipcp_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("recv [Protocol IPCP, Id = %02X, Code = '%s'", pstHdr->ubIdentifier, get_cpcode_name((EN_CPCODE)pstHdr->ubCode));
#endif

	for (INT i = 0; i < CPCODE_NUM; i++)
	{
		if (lr_staNegoHandler[i].enCode == (EN_CPCODE)pstHdr->ubCode)
			if (lr_staNegoHandler[i].pfunHandler)
			{
				lr_staNegoHandler[i].pfunHandler(pstcbPPP, pubPacket, nPacketLen);
				return; 
			}
	}
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("]\r\n");
#endif
}

#endif

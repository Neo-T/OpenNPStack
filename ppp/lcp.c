#include "port/datatype.h"
#include "errors.h"
#include "utils.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"

#if SUPPORT_PPP
#include "ppp/negotiation.h"
#include "ppp/ppp.h"
#define SYMBOL_GLOBALS
#include "ppp/lcp.h"
#undef SYMBOL_GLOBALS

static BOOL send_packet(PSTCB_NETIFPPP pstcbPPP, UCHAR ubCode, UCHAR ubIdentifier, UCHAR *pubData, USHORT usDataLen, BOOL blIsWaitACK, EN_ERROR_CODE *penErrCode); 
static BOOL send_conf_req_packet(PSTCB_NETIFPPP pstcbPPP, EN_ERROR_CODE *penErrCode); 

//* LCP配置请求处理函数
static INT put_mru(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult);
static INT put_async_map(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult);
static INT put_magic_number(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult);
static INT put_pcompression(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult);
static INT put_accompression(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult);
static INT get_mru(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult);
static INT get_async_map(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult);
static INT get_auth_type(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult);
static INT get_magic_number(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult);
static INT get_pcompression(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult);
static INT get_accompression(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult);
static ST_LNCP_CONFREQ_ITEM lr_staConfReqItem[LCP_CONFREQ_NUM] =
{
	{ (UCHAR)MRU,           TRUE,  put_mru,			  get_mru }, 
	{ (UCHAR)ASYNCMAP,      TRUE,  put_async_map,	  get_async_map }, 
	{ (UCHAR)AUTHTYPE,      FALSE, NULL,			  get_auth_type }, 
	{ (UCHAR)QUALITY,       FALSE, NULL }, 
	{ (UCHAR)MAGICNUMBER,   TRUE,  put_magic_number,  get_magic_number }, 
	{ (UCHAR)PCOMPRESSION,  TRUE,  put_pcompression,  get_pcompression }, 
	{ (UCHAR)ACCOMPRESSION, TRUE,  put_accompression, get_accompression } 
};

//* LCP协商处理函数
static void conf_request(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void conf_ack(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void conf_nak(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void conf_reject(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void terminate_request(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void terminate_ack(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void code_reject(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void protocol_reject(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void echo_request(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void echo_reply(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void discard_req(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static const ST_LNCPNEGOHANDLER l_staNegoHandler[CPCODE_NUM] =
{
	{ CONFREQ, conf_request },
	{ CONFACK, conf_ack },
	{ CONFNAK, conf_nak },
	{ CONFREJ, conf_reject },
	{ TERMREQ, terminate_request },
	{ TERMACK, terminate_ack },
	{ CODEREJ, code_reject },
	{ PROTREJ, protocol_reject },
	{ ECHOREQ, echo_request },
	{ ECHOREP, echo_reply },
	{ DISCREQ, discard_req }
};

static INT put_mru(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult)
{
	pstNegoResult = pstNegoResult; //* 避免编译器警告

	PST_LCP_CONFREQ_MRU pstReq = (PST_LCP_CONFREQ_MRU)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)MRU;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + sizeof(pstReq->usMRU);

	USHORT usMRU = ENDIAN_CONVERTER_USHORT((USHORT)PPP_MRU);
#if SUPPORT_PRINTF
	printf(", MRU = %d Bytes", PPP_MRU);
#endif
	memcpy((UCHAR *)&pstReq->usMRU, (UCHAR *)&usMRU, sizeof(USHORT));
	return (INT)pstReq->stHdr.ubLen;
}

static INT put_async_map(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult)
{
	pstNegoResult = pstNegoResult; //* 避免编译器警告

	PST_LCP_CONFREQ_ASYNCMAP pstReq = (PST_LCP_CONFREQ_ASYNCMAP)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)ASYNCMAP;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + 4;

	UINT unACCM = ENDIAN_CONVERTER_UINT((UINT)ACCM_INIT);
#if SUPPORT_PRINTF
	printf(", ACCM = %08X", unACCM);
#endif
	memcpy((UCHAR *)&pstReq->unMap, (UCHAR *)&unACCM, 4);
	return (INT)pstReq->stHdr.ubLen;
}

static INT put_magic_number(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult)
{
	PST_LCP_CONFREQ_MAGICNUMBER pstReq = (PST_LCP_CONFREQ_MAGICNUMBER)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)MAGICNUMBER;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + 4;

	srand(os_get_system_secs());
	pstNegoResult->stLCP.unMagicNum = (UINT)rand();
	memcpy((UCHAR *)&pstReq->unNum, &pstNegoResult->stLCP.unMagicNum, 4);
	return (INT)pstReq->stHdr.ubLen;
}

static INT put_pcompression(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult)
{
	pstNegoResult = pstNegoResult; //* 避免编译器警告

	PST_LCP_CONFREQ_PCOMPRESSION pstReq = (PST_LCP_CONFREQ_PCOMPRESSION)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)PCOMPRESSION;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR);

#if SUPPORT_PRINTF
	printf(", Protocol Field Compression");
#endif

	return (INT)pstReq->stHdr.ubLen;
}

static INT put_accompression(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult)
{
	pstNegoResult = pstNegoResult; //* 避免编译器警告

	PST_LCP_CONFREQ_ACCOMPRESSION pstReq = (PST_LCP_CONFREQ_ACCOMPRESSION)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)ACCOMPRESSION;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR);

#if SUPPORT_PRINTF
	printf(", Address/Control Field Compression");
#endif

	return (INT)pstReq->stHdr.ubLen;
}

static INT get_mru(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult)
{
	PST_LCP_CONFREQ_MRU pstItem = (PST_LCP_CONFREQ_MRU)pubItem;
	USHORT usVal = ENDIAN_CONVERTER_USHORT(pstItem->usMRU);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&usVal, sizeof(pstItem->usMRU));
	pstNegoResult->stLCP.usMRU = usVal;

#if SUPPORT_PRINTF
	printf(", MRU = %d", usVal);
#endif

	return (INT)pstItem->stHdr.ubLen;
}

static INT get_async_map(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult)
{
	PST_LCP_CONFREQ_ASYNCMAP pstItem = (PST_LCP_CONFREQ_ASYNCMAP)pubItem;
	UINT unVal = ENDIAN_CONVERTER_UINT(pstItem->unMap);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&unVal, sizeof(pstItem->unMap));
	pstNegoResult->stLCP.unACCM = unVal;

#if SUPPORT_PRINTF
	printf(", ACCM = %08X", unVal);
#endif

	return (INT)pstItem->stHdr.ubLen;
}

static INT get_auth_type(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult)
{
	PST_LCP_CONFREQ_AUTHTYPE_HDR pstItem = (PST_LCP_CONFREQ_AUTHTYPE_HDR)pubItem;
	USHORT usVal = ENDIAN_CONVERTER_USHORT(pstItem->usType);
	if (pubVal)
	{
		memcpy(pubVal, (UCHAR *)&usVal, sizeof(pstItem->usType));
		pubVal[sizeof(pstItem->usType)] = pubItem[sizeof(ST_LCP_CONFREQ_AUTHTYPE_HDR)];
	}
	pstNegoResult->stLCP.stAuth.usType = usVal;
	memcpy(pstNegoResult->stLCP.stAuth.ubaData, pubItem + sizeof(ST_LCP_CONFREQ_AUTHTYPE_HDR), (size_t)pstItem->stHdr.ubLen - sizeof(ST_LCP_CONFREQ_AUTHTYPE_HDR));

#if SUPPORT_PRINTF
	printf(", Authentication type = '%s'", get_protocol_name(usVal));
#endif

	return (INT)pstItem->stHdr.ubLen;
}

static INT get_magic_number(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult)
{
	pstNegoResult = pstNegoResult; //* 避免编译器警告

	PST_LCP_CONFREQ_MAGICNUMBER pstItem = (PST_LCP_CONFREQ_MAGICNUMBER)pubItem;
	//UINT unVal = UINT32_EDGE_CONVERT(pstItem->unNum);
	if (pubVal)
		memcpy(pubVal, (UCHAR *)&pstItem->unNum, sizeof(pstItem->unNum));

	return (INT)pstItem->stHdr.ubLen;
}

static INT get_pcompression(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult)
{
	PST_LCP_CONFREQ_PCOMPRESSION pstItem = (PST_LCP_CONFREQ_PCOMPRESSION)pubItem;
	pstNegoResult->stLCP.blIsProtoComp = TRUE;
	pstNegoResult->stLCP.blIsNegoValOfProtoComp = TRUE;

#if SUPPORT_PRINTF
	printf(", Protocol Field Compression");
#endif

	return (INT)pstItem->stHdr.ubLen;
}

static INT get_accompression(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult)
{
	PST_LCP_CONFREQ_ACCOMPRESSION pstItem = (PST_LCP_CONFREQ_ACCOMPRESSION)pubItem;
	pstNegoResult->stLCP.blIsAddrCtlComp = TRUE;
	pstNegoResult->stLCP.blIsNegoValOfAddrCtlComp = TRUE;

#if SUPPORT_PRINTF
	printf(", Address/Control Field Compression");
#endif

	return (INT)pstItem->stHdr.ubLen;
}

static void conf_request(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

	BOOL blIsFound;
	INT nReadIdx = sizeof(ST_LNCP_HDR);
	while (nReadIdx < nPacketLen && blIsFound)
	{
		//* 理论上如果出现ppp实现不支持的请求配置项应该是当前的实现远远落后于ppp标准的发展，此时必须确保循环能够安全退出，当然，实际情况是应该每次for循环都能够找到对应的处理函数
		blIsFound = FALSE;
		for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == lr_staConfReqItem[i].ubType)
			{
				if (lr_staConfReqItem[i].pfunGet)
					nReadIdx += lr_staConfReqItem[i].pfunGet(pubPacket + nReadIdx, NULL, pstcbPPP->pstNegoResult);
				blIsFound = TRUE;
			}
		}
	}
#if SUPPORT_PRINTF
	if(blIsFound)
		printf("]\r\nsent [Protocol LCP, Id = %02X, Code = 'Configure Ack']\r\n", pstHdr->ubIdentifier);
	else
		printf(", code <%02X> is not supported]\r\nsent [Protocol LCP, Id = %02X, Code = 'Configure Ack']\r\n", pubPacket[nReadIdx], pstHdr->ubIdentifier);
#endif	

	send_packet(pstcbPPP, (UCHAR)CONFACK, pstHdr->ubIdentifier, pubPacket + sizeof(ST_LNCP_HDR), nReadIdx, FALSE, NULL);
}

static void conf_ack(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{	
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket; 

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, PPP_LCP, pstHdr->ubIdentifier); 
#if SUPPORT_PRINTF
	printf("]\r\nuse %s authentication, magic number <%08X>\r\n", get_protocol_name(pstcbPPP->pstNegoResult->stLCP.stAuth.usType), pstcbPPP->pstNegoResult->stLCP.unMagicNum); 
#endif
}

static void conf_nak(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, PPP_LCP, pstHdr->ubIdentifier);
	
	//* 这个地方不会出现配置项不被支持的情况，因为这个是对端反馈的“我方”发送的配置请求项的数据域内容不被认可的应答报文
	INT nReadIdx = sizeof(ST_LNCP_HDR);
	while (nReadIdx < nPacketLen)
	{
		for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == lr_staConfReqItem[i].ubType)
				if (lr_staConfReqItem[i].pfunGet)
					nReadIdx += lr_staConfReqItem[i].pfunGet(pubPacket + nReadIdx, NULL, pstcbPPP->pstNegoResult);
		}
	}

	//* 再次发送协商请求报文，区别与上次发送的内容，此次请求已经把对端要求修改的数据域内容填充到报文中
	send_conf_req_packet(pstcbPPP, NULL); 
}

static void conf_reject(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, PPP_LCP, pstHdr->ubIdentifier);

	//* 同样这个地方不会出现配置项不被支持的情况，这个是对端反馈的不被支持的请求项，需要我们去除这些项然后再次发送协商请求
	INT nReadIdx = sizeof(ST_LNCP_HDR);
	while (nReadIdx < nPacketLen)
	{
		for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == lr_staConfReqItem[i].ubType)
				lr_staConfReqItem[i].blIsNegoRequired = FALSE;  //* 从协商配置请求中删除
		}
	}

	//* 再次发送协商请求报文，区别与上次发送的内容，此次请求已经把对端要求修改的数据域内容填充到报文中
	send_conf_req_packet(pstcbPPP, NULL);
}

static void terminate_request(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	CHAR szBuf[24];
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;
	
#if SUPPORT_PRINTF
	memcpy(szBuf, pubPacket + sizeof(ST_LNCP_HDR), (size_t)pstHdr->usLen - sizeof(ST_LNCP_HDR)); 
	szBuf[pstHdr->usLen - sizeof(ST_LNCP_HDR)] = 0; 
	printf(" \"%s\"]\r\n", szBuf); 
#endif

	send_packet(pstcbPPP, (UCHAR)TERMACK, pstHdr->ubIdentifier, (UCHAR *)szBuf, (USHORT)sizeof(ST_LNCP_HDR), FALSE, NULL);

	pstcbPPP->enState = TERMINATED;
}

static void terminate_ack(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{	
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

	wait_ack_list_del(&pstcbPPP->stWaitAckList, PPP_LCP, pstHdr->ubIdentifier);

#if SUPPORT_PRINTF
	printf("]\r\nLink terminated.\r\nppp0 <-/-> %s\r\n", get_ppp_port_name(pstcbPPP->hTTY));
#endif

	pstcbPPP->enState = TERMINATED; 
}

static void code_reject(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

#if SUPPORT_PRINTF
	printf("]\r\nerror: lcp code value not recognized by the peer.\r\n");
#endif

	pstcbPPP->enState = STACKFAULT;
}

static void protocol_reject(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

#if SUPPORT_PRINTF
	printf("]\r\nerror: ppp protocol value not recognized by the peer.\r\n");
#endif

	pstcbPPP->enState = STACKFAULT;
}

static void echo_request(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	UCHAR ubaData[64]; 

	
}

static void echo_reply(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{

}

static void discard_req(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{	
#if SUPPORT_PRINTF
	printf("]\r\n");
#endif
}

static BOOL send_packet(PSTCB_NETIFPPP pstcbPPP, UCHAR ubCode, UCHAR ubIdentifier, UCHAR *pubData, USHORT usDataLen, BOOL blIsWaitACK, EN_ERROR_CODE *penErrCode)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubData; 
	pstHdr->ubCode = ubCode;
	pstHdr->ubIdentifier = ubIdentifier; 
	pstHdr->usLen = ENDIAN_CONVERTER_USHORT(usDataLen);

	//* 申请一个buf list节点
	SHORT sBufListHead = -1;
	SHORT sNode = buf_list_get_ext(pubData, usDataLen, penErrCode);
	if (sNode < 0)
		return FALSE;
	buf_list_put_head(&sBufListHead, sNode);

	//* 发送
	INT nRtnVal = ppp_send(pstcbPPP->hTTY, LCP, sBufListHead, penErrCode);

	//* 释放刚才申请的buf list节点
	buf_list_free(sNode);
	
	//* 大于0意味着发送成功
	if (nRtnVal > 0)
	{
		//* 需要等待应答则将其加入等待队列
		if (blIsWaitACK)
			return wait_ack_list_add(&pstcbPPP->stWaitAckList, PPP_LCP, ubCode, ubIdentifier, 6, penErrCode);

		return TRUE; 
	}

	return FALSE; 
}

static BOOL send_conf_req_packet(PSTCB_NETIFPPP pstcbPPP, EN_ERROR_CODE *penErrCode)
{
	UCHAR ubIdentifier = pstcbPPP->pstNegoResult->ubIdentifier++;
#if SUPPORT_PRINTF
	printf("sent [Protocol LCP, Id = %02X, Code = 'Configure Request'", ubIdentifier);
#endif

	UCHAR ubaPacket[64];
	USHORT usWriteIdx = (USHORT)sizeof(ST_LNCP_HDR);
	for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
	{
		if (lr_staConfReqItem[i].blIsNegoRequired && lr_staConfReqItem[i].pfunPut)
			usWriteIdx += lr_staConfReqItem[i].pfunPut(&ubaPacket[usWriteIdx], pstcbPPP->pstNegoResult);
	}
	printf("]\r\n");

	return send_packet(pstcbPPP, (UCHAR)CONFREQ, ubIdentifier, ubaPacket, usWriteIdx, TRUE, penErrCode);
}

BOOL start_negotiation(PSTCB_NETIFPPP pstcbPPP, EN_ERROR_CODE *penErrCode)
{
	if (!wait_ack_list_init(&pstcbPPP->stWaitAckList, penErrCode))
		return FALSE;

	return send_conf_req_packet(pstcbPPP, penErrCode); 
}

BOOL send_terminate_req(PSTCB_NETIFPPP pstcbPPP, EN_ERROR_CODE *penErrCode)
{
#define TERM_REQ_STRING "Neo Request"
	UCHAR ubaPacket[32]; 

	UCHAR ubIdentifier = pstcbPPP->pstNegoResult->ubIdentifier++;
	UCHAR ubDataLen = sizeof(TERM_REQ_STRING) - 1; 
	memcpy(&ubaPacket[sizeof(ST_LNCP_HDR)], TERM_REQ_STRING, (size_t)ubDataLen);
	return send_packet(pstcbPPP, (UCHAR)TERMREQ, ubIdentifier, ubaPacket, (USHORT)(sizeof(ST_LNCP_HDR) + (size_t)ubDataLen), TRUE, penErrCode); 
}

void lcp_recv(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen, EN_ERROR_CODE *penErrCode)
{	
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket; 

#if SUPPORT_PRINTF
	printf("recv [Protocol LCP, Id = %02X, ", pstHdr->ubIdentifier); 
#endif

	for (INT i = 0; i < CPCODE_NUM; i++)
	{
		if (l_staNegoHandler[i].enCode == (EN_CPCODE)pstHdr->ubCode)
			if (l_staNegoHandler[i].pfunHandler)			
				return l_staNegoHandler[i].pfunHandler(pstcbPPP, pubPacket, nPacketLen);
	}
#if SUPPORT_PRINTF
	printf("]\r\n");
#endif
}

#endif
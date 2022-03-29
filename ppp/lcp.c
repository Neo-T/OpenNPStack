#include "port/datatype.h"
#include "errors.h"
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
static const ST_LNCP_CONFREQ_ITEM lr_staConfReqItem[LCP_CONFREQ_NUM] =
{
	{ (UCHAR)MRU,           TRUE,  put_mru,			  get_mru }, 
	{ (UCHAR)ASYNCMAP,      TRUE,  put_async_map,	  get_async_map }, 
	{ (UCHAR)AUTHTYPE,      FALSE, NULL,			  get_auth_type }, 
	{ (UCHAR)QUALITY,       FALSE, NULL }, 
	{ (UCHAR)MAGICNUMBER,   TRUE,  put_magic_number,  get_magic_number }, 
	{ (UCHAR)PCOMPRESSION,  TRUE,  put_pcompression,  get_pcompression }, 
	{ (UCHAR)ACCOMPRESSION, TRUE,  put_accompression, get_accompression } 
};

static INT put_mru(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult)
{
	pstNegoResult = pstNegoResult; //* 避免编译器警告

	PST_LCP_CONFREQ_MRU pstReq = (PST_LCP_CONFREQ_MRU)pubFilled;
	pstReq->stHdr.ubType = (UCHAR)MRU;
	pstReq->stHdr.ubLen = sizeof(ST_LNCP_CONFREQ_HDR) + sizeof(pstReq->usMRU);

	USHORT usMRU = USHORT_EDGE_CONVERT((USHORT)PPP_MRU);
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

	UINT unACCM = UINT32_EDGE_CONVERT((UINT)ACCM_INIT);
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
	USHORT usVal = USHORT_EDGE_CONVERT(pstItem->usMRU);
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
	UINT unVal = UINT32_EDGE_CONVERT(pstItem->unMap);
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
	USHORT usVal = USHORT_EDGE_CONVERT(pstItem->usType);
	if (pubVal)
	{
		memcpy(pubVal, (UCHAR *)&usVal, sizeof(pstItem->usType));
		pubVal[sizeof(pstItem->usType)] = pubItem[sizeof(ST_LCP_CONFREQ_AUTHTYPE_HDR)];
	}
	pstNegoResult->stLCP.stAuth.usType = usVal;
	memcpy(pstNegoResult->stLCP.stAuth.ubaData, pubItem + sizeof(ST_LCP_CONFREQ_AUTHTYPE_HDR), (size_t)pstItem->stHdr.ubLen - sizeof(ST_LCP_CONFREQ_AUTHTYPE_HDR));

#if SUPPORT_PRINTF
	printf(", Authentication type = '%s'", GetProtocolName(usVal));
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

static BOOL send_packet(HTTY hTTY, UCHAR ubCode, UCHAR ubIdentifier, UCHAR *pubData, USHORT usDataLen, BOOL blIsWaitACK, PST_PPPWAITACKLIST pstWAList, EN_ERROR_CODE *penErrCode)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubData; 
	pstHdr->ubCode = ubCode;
	pstHdr->ubIdentifier = ubIdentifier; 
	pstHdr->usLen = USHORT_EDGE_CONVERT(usDataLen);

	//* 申请一个buf list节点
	SHORT sBufListHead = -1;
	SHORT sNode = buf_list_get_ext(pubData, usDataLen, penErrCode);
	if (sNode < 0)
		return FALSE;
	buf_list_put_head(&sBufListHead, sNode);

	//* 发送
	INT nRtnVal = ppp_send(hTTY, LCP, sBufListHead, penErrCode);

	//* 释放刚才申请的buf list节点
	buf_list_free(sNode);
	
	//* 大于0意味着发送成功
	if (nRtnVal > 0)
	{
		//* 需要等待应答则将其加入等待队列
		if (blIsWaitACK)
			return wait_ack_list_add(pstWAList, PPP_LCP, ubCode, ubIdentifier, 6, penErrCode); 

		return TRUE; 
	}

	return FALSE; 
}

BOOL start_negotiation(HTTY hTTY, PST_PPPWAITACKLIST pstWAList, PST_PPPNEGORESULT pstNegoResult, EN_ERROR_CODE *penErrCode)
{
	if (!wait_ack_list_init(pstWAList, penErrCode))
		return FALSE;

	UCHAR ubIdentifier = pstNegoResult->ubIdentifier++;
#if SUPPORT_PRINTF
	printf("sent [Protocol LCP, Id = %02X, Code = 'Configure Request'", ubIdentifier);
#endif

	UCHAR ubaPacket[64];
	USHORT usWriteIdx = (USHORT)sizeof(ST_LNCP_HDR);
	for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
	{
		if (lr_staConfReqItem[i].blIsNegoRequired && lr_staConfReqItem[i].pfunPut)
			usWriteIdx += lr_staConfReqItem[i].pfunPut(&ubaPacket[usWriteIdx], pstNegoResult);
	}
	printf("]\r\n");

	return send_packet(hTTY, (UCHAR)CONFREQ, ubIdentifier, ubaPacket, usWriteIdx, TRUE, pstWAList, penErrCode); 
}

#endif
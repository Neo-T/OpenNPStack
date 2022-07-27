#include "port/datatype.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "onps_errors.h"
#include "onps_utils.h"
#include "mmu/buf_list.h"

#if SUPPORT_PPP
#include "ppp/negotiation.h"
#define SYMBOL_GLOBALS
#include "ppp/lcp.h"
#undef SYMBOL_GLOBALS
#include "ppp/ppp.h"

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
static ST_LNCP_CONFREQ_ITEM l_staConfReqItem[LCP_CONFREQ_NUM] =
{
	{ (UCHAR)MRU,           "mru",				 TRUE,  put_mru,		   get_mru }, 
	{ (UCHAR)ASYNCMAP,      "accm",				 TRUE,  put_async_map,	   get_async_map }, 
	{ (UCHAR)AUTHTYPE,      "Auth",				 FALSE, NULL,			   get_auth_type }, 
	{ (UCHAR)QUALITY,       "Quality",			 FALSE, NULL }, 
	{ (UCHAR)MAGICNUMBER,   "Magic",			 TRUE,  put_magic_number,  get_magic_number }, 
	{ (UCHAR)PCOMPRESSION,  "Proto Compress",	 TRUE,  put_pcompression,  get_pcompression }, 
	{ (UCHAR)ACCOMPRESSION, "Addr/Ctl Compress", TRUE,  put_accompression, get_accompression } 
};

//* LCP协商处理函数
static void conf_request(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void conf_ack(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void conf_nak(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void conf_reject(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void terminate_request(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void terminate_ack(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void code_reject(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void protocol_reject(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void echo_request(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void echo_reply(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static void discard_req(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
static const ST_LNCPNEGOHANDLER lr_staNegoHandler[CPCODE_NUM] =
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
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
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
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
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

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
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

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
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

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
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

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
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

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
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

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf(", Protocol Field Compression");
#endif

	return (INT)pstItem->stHdr.ubLen;
}

static INT get_accompression(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult)
{
	PST_LCP_CONFREQ_ACCOMPRESSION pstItem = (PST_LCP_CONFREQ_ACCOMPRESSION)pubItem;
	pstNegoResult->stLCP.blIsAddrCtlComp = TRUE;
	pstNegoResult->stLCP.blIsNegoValOfAddrCtlComp = TRUE;

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf(", Address/Control Field Compression");
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
		//* 理论上如果出现ppp实现不支持的请求配置项应该是当前的ppp栈实现远远落后于ppp标准的发展，此时必须确保循环能够安全退出，当然，实际情况是应该每次for循环都能够找到对应的处理函数
		blIsFound = FALSE;
		for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == l_staConfReqItem[i].ubType)
			{
				if (l_staConfReqItem[i].pfunGet)
					nReadIdx += l_staConfReqItem[i].pfunGet(pubPacket + nReadIdx, NULL, pstcbPPP->pstNegoResult);
				blIsFound = TRUE;
			}
		}
	}
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	if(blIsFound)
		printf("]\r\nsent [Protocol LCP, Id = %02X, Code = 'Configure Ack']\r\n", pstHdr->ubIdentifier);
	else
		printf(", code <%02X> is not supported]\r\nsent [Protocol LCP, Id = %02X, Code = 'Configure Ack']\r\n", pubPacket[nReadIdx], pstHdr->ubIdentifier);
#endif	

	send_nego_packet(pstcbPPP, LCP, (UCHAR)CONFACK, pstHdr->ubIdentifier, pubPacket, nReadIdx, FALSE, NULL);
}

static void conf_ack(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{	
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket; 

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, LCP, pstHdr->ubIdentifier); 
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("]\r\nuse %s authentication, magic number <%08X>\r\n", get_protocol_name(pstcbPPP->pstNegoResult->stLCP.stAuth.usType), pstcbPPP->pstNegoResult->stLCP.unMagicNum); 
#endif
	
	pstcbPPP->enState = STARTAUTHEN; 
}

static void conf_nak(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, LCP, pstHdr->ubIdentifier);
	
	//* 这个地方不会出现配置项不被支持的情况，因为这个是对端反馈的“我方”发送的配置请求项的数据域内容不被认可的应答报文
	INT nReadIdx = sizeof(ST_LNCP_HDR);
	while (nReadIdx < nPacketLen)
	{
		for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == l_staConfReqItem[i].ubType)
				if (l_staConfReqItem[i].pfunGet)
					nReadIdx += l_staConfReqItem[i].pfunGet(pubPacket + nReadIdx, NULL, pstcbPPP->pstNegoResult);			
		}
	}

	//* 再次发送协商请求报文，区别与上次发送的内容，此次请求已经把对端要求修改的数据域内容填充到报文中
	lcp_send_conf_request(pstcbPPP, NULL); 
}

static void conf_reject(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, LCP, pstHdr->ubIdentifier);

	//* 同样这个地方不会出现配置项不被支持的情况，这个是对端反馈的不被支持的请求项，需要我们去除这些项然后再次发送协商请求
	INT nReadIdx = sizeof(ST_LNCP_HDR);
	while (nReadIdx < nPacketLen)
	{
		for (INT i = 0; i < LCP_CONFREQ_NUM; i++)
		{
			if (pubPacket[nReadIdx] == l_staConfReqItem[i].ubType)
			{
				nReadIdx += (INT)pubPacket[nReadIdx + 1]; 

		#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
				printf(", %s", l_staConfReqItem[i].pszName); 
		#endif
				l_staConfReqItem[i].blIsNegoRequired = FALSE;  //* 从协商配置请求中删除
			}
		}
	}

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("]\r\n");
#endif

	//* 再次发送协商请求报文，区别与上次发送的内容，此次请求已经把对端要求修改的数据域内容填充到报文中
	lcp_send_conf_request(pstcbPPP, NULL);
}

static void terminate_request(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	CHAR szBuf[24];
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;
	
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	UINT unCpyBytes = (size_t)pstHdr->usLen - sizeof(ST_LNCP_HDR); 
	unCpyBytes = (unCpyBytes < sizeof(szBuf) - 1) ? unCpyBytes : sizeof(szBuf) - 1;
	memcpy(szBuf, pubPacket + sizeof(ST_LNCP_HDR), unCpyBytes);
	szBuf[unCpyBytes] = 0;
	printf(" Data = \"%s\"]\r\n", szBuf); 
#endif

	printf("sent [Protocol LCP, Id = %02X, Code = 'Terminate Ack']", pstHdr->ubIdentifier); 
	send_nego_packet(pstcbPPP, LCP, (UCHAR)TERMACK, pstHdr->ubIdentifier, (UCHAR *)szBuf, (USHORT)sizeof(ST_LNCP_HDR), FALSE, NULL);

	pstcbPPP->enState = TERMINATED;
}

static void terminate_ack(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{	
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, LCP, pstHdr->ubIdentifier);

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("]\r\nLink terminated.\r\nppp0 <-/-> %s\r\n", get_ppp_port_name(pstcbPPP->hTTY));
#endif

	pstcbPPP->enState = TERMINATED; 
}

static void code_reject(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;	

	//* 无论何种情形，收到应答都应该先清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, PPP_LCP, pstHdr->ubIdentifier);

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    UCHAR ubRejCode = pubPacket[sizeof(ST_LNCP_HDR)];
	printf(", Reject Code = \"%s\", hex val = %02X]\r\nerror: lcp code value not recognized by the peer.\r\n", get_cpcode_name((EN_CPCODE)ubRejCode), ubRejCode); 
#endif

	pstcbPPP->enState = STACKFAULT;
}

static void protocol_reject(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;
	USHORT usRejProtocol; 

	//* 无论何种情形，收到应答都应该先清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, LCP, pstHdr->ubIdentifier);

	((UCHAR *)&usRejProtocol)[0] = pubPacket[sizeof(ST_LNCP_HDR) + 1]; 
	((UCHAR *)&usRejProtocol)[1] = pubPacket[sizeof(ST_LNCP_HDR)];

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf(", Reject Protocol %s, hex val = %04X]\r\nerror: ppp protocol value not recognized by the peer.\r\n", get_protocol_name(usRejProtocol), usRejProtocol);
#endif

	pstcbPPP->enState = STACKFAULT;
}

static void echo_request(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	CHAR szData[80];
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;
	PST_LCP_ECHO_REQ_HDR pstReqHdr = (PST_LCP_ECHO_REQ_HDR)(pubPacket + sizeof(ST_LNCP_HDR)); 
    UINT unCpyBytes = pstHdr->usLen - sizeof(ST_LNCP_HDR) - sizeof(ST_LCP_ECHO_REQ_HDR);

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1	
	unCpyBytes = unCpyBytes < sizeof(szData) - 1 ? unCpyBytes : sizeof(szData) - 1; 
	memcpy(szData, pubPacket + sizeof(ST_LNCP_HDR) + sizeof(ST_LCP_ECHO_REQ_HDR), unCpyBytes); 
	szData[unCpyBytes] = 0; 
	printf(", Remote Magic = <%08X>, Data = \"%s\"]\r\n", pstReqHdr->unLocalMagicNum, szData); 	
#endif

	//* 发送应答报文
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("sent [Protocol LCP, Id = %02X, Code = 'Echo Reply', Remote Magic = <%08X>, Local Magic = <%08X>, Data = \"%s\"]", pstHdr->ubIdentifier, pstReqHdr->unLocalMagicNum, pstcbPPP->pstNegoResult->stLCP.unMagicNum, szData);
#endif

	PST_LCP_ECHO_REPLY_HDR pstReplyHdr = (PST_LCP_ECHO_REPLY_HDR)&szData[sizeof(ST_LNCP_HDR)]; 
	pstReplyHdr->unLocalMagicNum = pstReqHdr->unLocalMagicNum; 
	pstReplyHdr->unRemoteMagicNum = pstcbPPP->pstNegoResult->stLCP.unMagicNum;     
	unCpyBytes = unCpyBytes < sizeof(szData) - sizeof(ST_LNCP_HDR) - sizeof(ST_LCP_ECHO_REPLY_HDR) ? unCpyBytes : sizeof(szData) - sizeof(ST_LNCP_HDR) - sizeof(ST_LCP_ECHO_REPLY_HDR); 
	memcpy(&szData[sizeof(ST_LNCP_HDR) + sizeof(ST_LCP_ECHO_REPLY_HDR)], pubPacket + sizeof(ST_LNCP_HDR) + sizeof(ST_LCP_ECHO_REQ_HDR), unCpyBytes);
	send_nego_packet(pstcbPPP, LCP, (UCHAR)ECHOREP, pstHdr->ubIdentifier, (UCHAR *)szData, sizeof(ST_LNCP_HDR) + sizeof(ST_LCP_ECHO_REPLY_HDR) + unCpyBytes, FALSE, NULL);
}

static void echo_reply(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{	
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;	

	//* 收到应答则清除等待队列节点
	wait_ack_list_del(&pstcbPPP->stWaitAckList, LCP, pstHdr->ubIdentifier); 

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    PST_LCP_ECHO_REPLY_HDR pstReplyHdr = (PST_LCP_ECHO_REPLY_HDR)(pubPacket + sizeof(ST_LNCP_HDR)); 
    CHAR szData[64];
	UINT unCpyBytes = ENDIAN_CONVERTER_USHORT(pstHdr->usLen) - sizeof(ST_LNCP_HDR) - sizeof(ST_LCP_ECHO_REPLY_HDR);
	unCpyBytes = unCpyBytes < sizeof(szData) - 1 ? unCpyBytes : sizeof(szData) - 1; 
	memcpy(szData, pubPacket + sizeof(ST_LNCP_HDR) + sizeof(ST_LCP_ECHO_REPLY_HDR), unCpyBytes);
	szData[unCpyBytes] = 0; 
#if PRINTF_THREAD_MUTEX
    os_thread_mutex_lock(o_hMtxPrintf);
#endif
	printf("recv [Protocol LCP, Id = %02X, Code = 'Echo Reply', Local Magic = <%08X>, Remote Magic = <%08X>, Data = \"%s\"]\r\n", pstHdr->ubIdentifier, pstReplyHdr->unLocalMagicNum, pstReplyHdr->unRemoteMagicNum, szData);
#if PRINTF_THREAD_MUTEX
    os_thread_mutex_unlock(o_hMtxPrintf);
#endif
#endif
}

static void discard_req(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{	
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("]\r\n");
#endif
}

BOOL lcp_send_conf_request(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr)
{
	UCHAR ubIdentifier = pstcbPPP->pstNegoResult->ubIdentifier++;
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("sent [Protocol LCP, Id = %02X, Code = 'Configure Request'", ubIdentifier);
#endif

	UCHAR ubaPacket[64];
	USHORT usWriteIdx = (USHORT)sizeof(ST_LNCP_HDR);
	INT i; 
	for (i = 0; i < LCP_CONFREQ_NUM; i++)
	{
		if (l_staConfReqItem[i].blIsNegoRequired && l_staConfReqItem[i].pfunPut)
			usWriteIdx += l_staConfReqItem[i].pfunPut(&ubaPacket[usWriteIdx], pstcbPPP->pstNegoResult);
	}
	printf("]\r\n");

	return send_nego_packet(pstcbPPP, LCP, (UCHAR)CONFREQ, ubIdentifier, ubaPacket, usWriteIdx, TRUE, penErr);
}

BOOL lcp_start_negotiation(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr)
{
	if (!wait_ack_list_init(&pstcbPPP->stWaitAckList, penErr))
		return FALSE;

	return lcp_send_conf_request(pstcbPPP, penErr);
}

void lcp_end_negotiation(PSTCB_PPP pstcbPPP)
{
	wait_ack_list_uninit(&pstcbPPP->stWaitAckList); 
}

BOOL lcp_send_terminate_req(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr)
{
#define TERM_REQ_STRING "Bye, Trinity"
	UCHAR ubaPacket[64]; 

	UCHAR ubIdentifier = pstcbPPP->pstNegoResult->ubIdentifier++;
	UCHAR ubDataLen = sizeof(TERM_REQ_STRING) - 1; 
	memcpy(&ubaPacket[sizeof(ST_LNCP_HDR)], TERM_REQ_STRING, (size_t)ubDataLen);

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("sent [Protocol LCP, Id = %02X, Code = 'Terminate Request', Data = \"%s\"]\r\n", ubIdentifier, TERM_REQ_STRING);
#endif
	return send_nego_packet(pstcbPPP, LCP, (UCHAR)TERMREQ, ubIdentifier, ubaPacket, (USHORT)(sizeof(ST_LNCP_HDR) + (size_t)ubDataLen), TRUE, penErr);
}

BOOL lcp_send_echo_request(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr)
{
#define ECHO_STRING	"Hello, I'm Neo!" 
	UCHAR ubaPacket[80]; 
	PST_LCP_ECHO_REQ_HDR pstReqHdr = (PST_LCP_ECHO_REQ_HDR)&ubaPacket[sizeof(ST_LNCP_HDR)];

	UCHAR ubIdentifier = pstcbPPP->pstNegoResult->ubIdentifier++;	

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    #if PRINTF_THREAD_MUTEX
    os_thread_mutex_lock(o_hMtxPrintf);
    #endif
	printf("sent [Protocol LCP, Id = %02X, Code = 'Echo Request', Magic = <%08X>, Data = \"%s\"]\r\n", ubIdentifier, pstcbPPP->pstNegoResult->stLCP.unMagicNum, ECHO_STRING);
    #if PRINTF_THREAD_MUTEX
    os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif

	//* 填充数据
	USHORT usDataLen = (USHORT)strlen(ECHO_STRING); 
	pstReqHdr->unLocalMagicNum = pstcbPPP->pstNegoResult->stLCP.unMagicNum; 
	memcpy(&ubaPacket[sizeof(ST_LNCP_HDR) + sizeof(ST_LCP_ECHO_REQ_HDR)], ECHO_STRING, usDataLen);
	usDataLen += sizeof(ST_LNCP_HDR) + sizeof(ST_LCP_ECHO_REQ_HDR); 

	return send_nego_packet(pstcbPPP, LCP, (UCHAR)ECHOREQ, ubIdentifier, ubaPacket, (USHORT)usDataLen, TRUE, penErr);
}

//* lcp协议接收函数
void lcp_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{	
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket; 

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    if(ECHOREP != pstHdr->ubCode)
	    printf("recv [Protocol LCP, Id = %02X, Code = '%s'", pstHdr->ubIdentifier, get_cpcode_name((EN_CPCODE)pstHdr->ubCode)); 
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
    if (ECHOREP != pstHdr->ubCode)
	    printf("]\r\n");
#endif
}

#endif

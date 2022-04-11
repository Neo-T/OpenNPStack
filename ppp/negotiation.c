#include "port/datatype.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "errors.h"
#include "mmu/buf_list.h"
#include "utils.h"

#if SUPPORT_PPP
#define SYMBOL_GLOBALS
#include "ppp/negotiation.h"
#undef SYMBOL_GLOBALS
#include "ppp/lcp.h"
#include "ppp/pap.h"
#include "ppp/ipcp.h"
#include "ppp/ppp.h"

BOOL send_nego_packet(PSTCB_PPP pstcbPPP, USHORT usProtocol, UCHAR ubCode, UCHAR ubIdentifier, UCHAR *pubData, USHORT usDataLen, BOOL blIsWaitACK, EN_ERROR_CODE *penErrCode)
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
	INT nRtnVal = ppp_send(pstcbPPP->hTTY, (EN_NPSPROTOCOL)usProtocol, sBufListHead, penErrCode);

	//* 释放刚才申请的buf list节点
	buf_list_free(sNode);

	//* 大于0意味着发送成功
	if (nRtnVal > 0)
	{
		//* 需要等待应答则将其加入等待队列
		if (blIsWaitACK)
		{
			pstcbPPP->stWaitAckList.ubIsTimeout = FALSE; 
			return wait_ack_list_add(&pstcbPPP->stWaitAckList, usProtocol, ubCode, ubIdentifier, 6, penErrCode);
		}

		return TRUE;
	}

	return FALSE;
}

static void ppp_negotiate(PSTCB_PPP pstcbPPP, EN_ERROR_CODE *penErrCode)
{
	switch (pstcbPPP->enState)
	{
	case LCPCONFREQ: 
		if (pstcbPPP->stWaitAckList.ubIsTimeout) //* 意味着没收到应答报文
		{
			if (!lcp_send_conf_request(pstcbPPP, penErrCode)) //* 再发送一次lcp配置请求报文
			{
		#if SUPPORT_PRINTF
				printf("lcp_send_conf_request() failed, %s\r\n", error(*penErrCode)); 
		#endif

				pstcbPPP->enState = STACKFAULT;
			}
		}
		break; 

	case STARTAUTHEN:
		if (pstcbPPP->pstNegoResult->stLCP.stAuth.usType == PPP_CHAP)
		{
			//* CHAP协议会在收到LCP协商通过的应答报文后，主动下发认证报文，在这里记录下开始时间，以避免ppp栈一直等待下去（如果链路已经异常断开的话）		
			pstcbPPP->pstNegoResult->unLastRcvedSecs = os_get_system_secs();
			pstcbPPP->enState = AUTHENTICATE;
		}
		else if (pstcbPPP->pstNegoResult->stLCP.stAuth.usType == PPP_PAP)	  
		{
			//* 发送认证报文
			if (pap_send_auth_request(pstcbPPP, penErrCode))
			{
				pstcbPPP->enState = AUTHENTICATE;
			}
			else
			{
		#if SUPPORT_PRINTF
				printf("pap_send_auth_request() failed, %s\r\n", error(*penErrCode));
		#endif

				pstcbPPP->enState = STACKFAULT;
			}
		}
		else
		{
		#if SUPPORT_PRINTF
			printf("error: unrecognized authentication protocol\r\n");
		#endif
			pstcbPPP->enState = STACKFAULT;
		}
			
		break; 

	case AUTHENTICATE: 
		if (pstcbPPP->pstNegoResult->stLCP.stAuth.usType == PPP_CHAP)
		{
			if (os_get_system_secs() - pstcbPPP->pstNegoResult->unLastRcvedSecs > 30) //* 如果超过30秒还未收到对端发送的认证报文那么在这里就认为链路已经异常断开了，需要重新建立链路了，此时我们只需把状态迁移到AUTHENTIMEOUT就可了
				pstcbPPP->enState = AUTHENTIMEOUT;
		}
		else
		{
			if (pstcbPPP->stWaitAckList.ubIsTimeout) //* 意味着没收到应答报文，需要回到STARTAUTHEN阶段再发送一次认证报文
				pstcbPPP->enState = STARTAUTHEN;
		}

		break; 

	case SENDIPCPCONFREQ: 
		if (ipcp_send_conf_request(pstcbPPP, penErrCode))
		{
			pstcbPPP->enState = WAITIPCPCONFACK;
		}
		else
		{
		#if SUPPORT_PRINTF
			printf("ipcp_send_conf_request() failed, %s\r\n", error(*penErrCode));
		#endif
			pstcbPPP->enState = STACKFAULT;
		}
		break; 

	case WAITIPCPCONFACK:
		if (pstcbPPP->stWaitAckList.ubIsTimeout) //* 意味着没收到应答报文
			pstcbPPP->enState = SENDIPCPCONFREQ;
	}
}

void ppp_link_establish(PSTCB_PPP pstcbPPP, EN_ERROR_CODE *penErrCode)
{
	switch (pstcbPPP->enState)
	{
	case STARTNEGOTIATION:
		if (lcp_start_negotiation(pstcbPPP, penErrCode))
			pstcbPPP->enState = NEGOTIATION;
		else
		{
#if SUPPORT_PRINTF

#endif
			pstcbPPP->enState = STACKFAULT;
			return;
		}

	default:
		ppp_negotiate(pstcbPPP, penErrCode);
		return;
	}
}

#endif

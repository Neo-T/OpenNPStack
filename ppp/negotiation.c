#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"

#if SUPPORT_PPP
#define SYMBOL_GLOBALS
#include "ppp/negotiation.h"
#undef SYMBOL_GLOBALS
#include "ppp/lcp.h"

BOOL send_nego_packet(PSTCB_NETIFPPP pstcbPPP, USHORT usProtocol, UCHAR ubCode, UCHAR ubIdentifier, UCHAR *pubData, USHORT usDataLen, BOOL blIsWaitACK, EN_ERROR_CODE *penErrCode)
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
			return wait_ack_list_add(&pstcbPPP->stWaitAckList, usProtocol, ubCode, ubIdentifier, 6, penErrCode);

		return TRUE;
	}

	return FALSE;
}

static void ppp_negotiate(PSTCB_NETIFPPP pstcbPPP, UINT *punStartSecs, EN_ERROR_CODE *penErrCode)
{
	if (pstcbPPP->stWaitAckList.ubTimeoutNum > WAIT_ACK_TIMEOUT_NUM)
		goto __lblErr; 

	switch (pstcbPPP->enState)
	{
	case LCPCONFREQ: 
		if (pstcbPPP->stWaitAckList.ubTimeoutNum > 0) //* 意味着没收到应答报文
		{
			if (!send_conf_request(pstcbPPP, penErrCode))
			{
		#if SUPPORT_PRINTF
				printf("send_conf_request() failed, %s\r\n", error(*penErrCode)); 
		#endif

				goto __lblErr;
			}
		}
		break; 

	case STARTAUTHEN:
		*punStartSecs = os_get_system_secs();
		pstcbPPP->enState = AUTHENTICATE; 
		break; 

	case AUTHENTICATE: 
		if(os_get_system_secs() - (*punStartSecs) > )
		break; 
	}

	return; 

__lblErr: 
	end_negotiation(pstcbPPP);
	pstcbPPP->enState = STACKFAULT;
	return;
}

void ppp_link_establish(PSTCB_NETIFPPP pstcbPPP, BOOL *pblIsRunning, EN_ERROR_CODE *penErrCode)
{
	UINT unStartSecs; 
	while (*pblIsRunning)
	{
		switch (pstcbPPP->enState)
		{
		case TTYINIT: 
			if (tty_ready(pstcbPPP->hTTY, penErrCode))
				pstcbPPP->enState = STARTNEGOTIATION; //* 这里不再break，直接进入下一个阶段，而不是循环一圈后再进入下一个阶段
			else
				return; 

		case STARTNEGOTIATION: 
			if (start_negotiation(pstcbPPP, penErrCode))
				pstcbPPP->enState = NEGOTIATION;
			else
			{
				pstcbPPP->enState = STACKFAULT;
				return;
			}

		case NEGOTIATION: 
			ppp_negotiate(pstcbPPP, &unStartSecs, penErrCode);
			return; 
		}
	}
}

#endif
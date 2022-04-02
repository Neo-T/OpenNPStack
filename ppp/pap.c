#include "port/datatype.h"
#include "errors.h"
#include "utils.h"
#include "md5.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"

#if SUPPORT_PPP
#include "ppp/negotiation.h"
#include "ppp/ppp.h"
#define SYMBOL_GLOBALS
#include "ppp/pap.h"
#undef SYMBOL_GLOBALS

BOOL pap_send_auth_request(PSTCB_NETIFPPP pstcbPPP, EN_ERROR_CODE *penErrCode)
{

	return TRUE; 
}

//* pap协议接收函数
void pap_recv(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

#if SUPPORT_PRINTF
	printf("recv [Protocol CHAP, Id = %02X, Code = '%s'", pstHdr->ubIdentifier, get_chap_code_name(pstHdr->ubCode));
#endif

	switch ((EN_CHAPCODE)pstHdr->ubCode)
	{
	case CHALLENGE: 
		pstcbPPP->pstNegoResult->unLastRcvedSecs = os_get_system_secs(); //* 收到challenge报文，更新接收时间，避免协商状态机报错
		return send_response(pstcbPPP, pubPacket, nPacketLen); 

	case SUCCEEDED:
		//* 收到应答则清除等待队列节点
		wait_ack_list_del(&pstcbPPP->stWaitAckList, PPP_LCP, pstHdr->ubIdentifier);
#if SUPPORT_PRINTF
		printf("]\r\nCHAP authentication succeeded. \r\n"); 
#endif 
		pstcbPPP->enState = IPCPCONFREQ;
		break; 

	case FAILURE:
		//* 收到应答则清除等待队列节点
		wait_ack_list_del(&pstcbPPP->stWaitAckList, PPP_LCP, pstHdr->ubIdentifier);
#if SUPPORT_PRINTF
		printf("]\r\nCHAP authentication failed.\r\n");
#endif
		pstcbPPP->enState = AUTHENFAILED;
		break; 

	case RESPONSE:
#if SUPPORT_PRINTF
		printf("]\r\nCHAP authentication failed, unsupported response type.\r\n");
#endif
		pstcbPPP->enState = AUTHENFAILED;
		break; 
	}
}

#endif
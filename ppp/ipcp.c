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

//* pap协议接收函数
void ipcp_recv(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;
	CHAR szMessage[32]; 

#if SUPPORT_PRINTF
	printf("recv [Protocol PAP, Id = %02X, Code = '%s'", pstHdr->ubIdentifier, get_pap_code_name(pstHdr->ubCode));
#endif

	switch ((EN_PAPCODE)pstHdr->ubCode)
	{
	case AUTHPASSED:
		//* 收到应答则清除等待队列节点
		wait_ack_list_del(&pstcbPPP->stWaitAckList, PPP_PAP, pstHdr->ubIdentifier);
#if SUPPORT_PRINTF
		get_message(pubPacket, nPacketLen, szMessage, sizeof(szMessage));
		printf(", Message = \"%s\"]\r\nPAP authentication succeeded. \r\n", szMessage);
#endif 
		pstcbPPP->enState = IPCPCONFREQ;
		break; 

	case AUTHREFUSED:
		//* 收到应答则清除等待队列节点
		wait_ack_list_del(&pstcbPPP->stWaitAckList, PPP_PAP, pstHdr->ubIdentifier);
#if SUPPORT_PRINTF
		get_message(pubPacket, nPacketLen, szMessage, sizeof(szMessage));
		printf(", Message = \"%s\"]\r\nPAP authentication failed.\r\n", szMessage);
#endif
		pstcbPPP->enState = AUTHENFAILED;
		break; 

	case AUTHREQ:
#if SUPPORT_PRINTF
		printf("]\r\nPAP authentication failed, unsupported request type.\r\n");
#endif
		pstcbPPP->enState = AUTHENFAILED;
		break; 
	}
}

#endif
#include "port/datatype.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "onps_errors.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "onps_md5.h"

#if SUPPORT_PPP
#include "ppp/negotiation.h"
#define SYMBOL_GLOBALS
#include "ppp/pap.h"
#undef SYMBOL_GLOBALS
#include "ppp/ppp.h"

BOOL pap_send_auth_request(PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr)
{
	UCHAR ubaPacket[64];
	UCHAR ubIdentifier = pstcbPPP->pstNegoResult->ubIdentifier++;

	const CHAR *pszUser, *pszPassword;
	get_ppp_auth_info(pstcbPPP->hTTY, &pszUser, &pszPassword);

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("sent [Protocol PAP, Id = %02X, Code = '%s', User = '%s']\r\n", ubIdentifier, get_pap_code_name(AUTHREQ), pszUser);
#endif
	USHORT usUserLen = strlen(pszUser), usPasswordLen = strlen(pszPassword); 
	USHORT usDataLen = sizeof(ST_LNCP_HDR); 
	ubaPacket[usDataLen] = (UCHAR)usUserLen; 
	usDataLen += 1; 
	memcpy(&ubaPacket[usDataLen], pszUser, usUserLen); 
	usDataLen += usUserLen; 
	ubaPacket[usDataLen] = (UCHAR)usPasswordLen;
	usDataLen += 1;
	memcpy(&ubaPacket[usDataLen], pszPassword, usPasswordLen);
	usDataLen += usPasswordLen; 

	return send_nego_packet(pstcbPPP, PAP, (UCHAR)AUTHREQ, ubIdentifier, ubaPacket, (USHORT)usDataLen, TRUE, penErr);
}

static void get_message(UCHAR *pubPacket, INT nPacketLen, CHAR *pszMessageBuf, UCHAR ubMessageBufLen)
{
	UCHAR ubMessageLen = pubPacket[sizeof(ST_LNCP_HDR)]; 
	UINT unCpyBytes = ubMessageLen < ubMessageBufLen - 1 ? (UINT)ubMessageLen : (UINT)ubMessageBufLen - 1; 
	memcpy(pszMessageBuf, pubPacket + sizeof(ST_LNCP_HDR) + 1, unCpyBytes); 
	pszMessageBuf[unCpyBytes] = 0;
}

//* pap协议接收函数
void pap_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	CHAR szMessage[32]; 
#endif

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
	printf("recv [Protocol PAP, Id = %02X, Code = '%s'", pstHdr->ubIdentifier, get_pap_code_name((EN_PAPCODE)pstHdr->ubCode));
#endif

	switch ((EN_PAPCODE)pstHdr->ubCode)
	{
	case AUTHPASSED:
		//* 收到应答则清除等待队列节点
		wait_ack_list_del(&pstcbPPP->stWaitAckList, PAP, pstHdr->ubIdentifier);
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
		get_message(pubPacket, nPacketLen, szMessage, sizeof(szMessage));
		printf(", Message = \"%s\"]\r\nPAP authentication succeeded. \r\n", szMessage);
#endif 
		pstcbPPP->enState = SENDIPCPCONFREQ;
		break; 

	case AUTHREFUSED:
		//* 收到应答则清除等待队列节点
		wait_ack_list_del(&pstcbPPP->stWaitAckList, PAP, pstHdr->ubIdentifier);
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
		get_message(pubPacket, nPacketLen, szMessage, sizeof(szMessage));
		printf(", Message = \"%s\"]\r\nPAP authentication failed.\r\n", szMessage);
#endif
		pstcbPPP->enState = AUTHENFAILED;
		break; 

	case AUTHREQ:
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
		printf("]\r\nPAP authentication failed, unsupported request type.\r\n");
#endif
		pstcbPPP->enState = AUTHENFAILED;
		break; 
	}
}

#endif

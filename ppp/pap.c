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
#define SYMBOL_GLOBALS
#include "ppp/pap.h"
#undef SYMBOL_GLOBALS
#include "ppp/ppp.h"

BOOL pap_send_auth_request(PSTCB_NETIFPPP pstcbPPP, EN_ERROR_CODE *penErrCode)
{
	UCHAR ubaPacket[64];
	UCHAR ubIdentifier = pstcbPPP->pstNegoResult->ubIdentifier++;

	const CHAR *pszUser, *pszPassword;
	get_ppp_auth_info(pstcbPPP->hTTY, &pszUser, &pszPassword);

#if SUPPORT_PRINTF
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

	return send_nego_packet(pstcbPPP, PPP_PAP, (UCHAR)AUTHREQ, ubIdentifier, ubaPacket, (USHORT)usDataLen, TRUE, penErrCode);
}

static void get_message(UCHAR *pubPacket, INT nPacketLen, CHAR *pszMessageBuf, UCHAR ubMessageBufLen)
{
	UCHAR ubMessageLen = pubPacket[sizeof(ST_LNCP_HDR)]; 
	UINT unCpyBytes = ubMessageLen < ubMessageBufLen - 1 ? (UINT)ubMessageLen : (UINT)ubMessageBufLen - 1; 
	memcpy(pszMessageBuf, pubPacket + sizeof(ST_LNCP_HDR) + 1, unCpyBytes); 
	pszMessageBuf[unCpyBytes] = 0;
}

//* pap协议接收函数
void pap_recv(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
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
		pstcbPPP->enState = SENDIPCPCONFREQ;
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
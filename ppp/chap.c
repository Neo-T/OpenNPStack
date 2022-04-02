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
#include "ppp/chap.h"
#undef SYMBOL_GLOBALS

static void send_response(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{	
	CHAR szData[64];
	PST_CHAP_DATA pstData = (PST_CHAP_DATA)(pubPacket + sizeof(ST_LNCP_HDR));
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;	

#if SUPPORT_PRINTF
	snprintf_hex(pstData->ubaChallenge, (USHORT)pstData->ubChallengeLen, szData, sizeof(szData) - 1, FALSE);
	printf(", Challenge = <%s>", szData);
	UINT unCpyBytes = nPacketLen - sizeof(ST_LNCP_HDR) - sizeof(ST_CHAP_DATA);
	unCpyBytes = unCpyBytes < sizeof(szData) - 1 ? unCpyBytes : sizeof(szData) - 1;
	memcpy(szData, pubPacket + sizeof(ST_LNCP_HDR) + sizeof(ST_CHAP_DATA), unCpyBytes);
	szData[unCpyBytes] = 0; 
	printf(", name = \"%s\"]\r\n", szData); 
#endif

	const CHAR *pszUser, *pszPassword; 
	PST_DIAL_AUTH_INFO pstAuth = get_ppp_dial_auth_info(pstcbPPP->hTTY);
	if (pstAuth)
	{
		pszUser = pstAuth->pszUser; 
		pszPassword = pstAuth->pszPassword; 
	}
	else
	{
		pszUser = AUTH_USER_DEFAULT;
		pszPassword = AUTH_PASSWORD_DEFAULT;
	}

	//* 使用MD5算法生成Challenge值：ubIdentifier + 拨号脚本设置的密码 + 对端下发的Challenge值
	UINT unOriginalLen, unPasswordLen = strlen(pszPassword);
	szData[0] = pstHdr->ubIdentifier; 
	unOriginalLen = 1; 
	memcpy(&szData[1], pszPassword, unPasswordLen); 
	unOriginalLen += unPasswordLen; 
	memcpy(&szData[unOriginalLen], pstData->ubaChallenge, pstData->ubChallengeLen);
	unOriginalLen += (UINT)pstData->ubChallengeLen;
	ST_MD5VAL stChallengeCode = md5(szData, unOriginalLen); 

	//* 封装报文
	USHORT usDataLen; 
	UINT unUserLen = strlen(pszUser);
	PST_CHAP_DATA pstRespData = (PST_CHAP_DATA)&szData[sizeof(ST_LNCP_HDR)]; 
	usDataLen = sizeof(ST_LNCP_HDR); 
	pstRespData->ubChallengeLen = CHAP_CHALLENGE_LEN; 
	memcpy(pstRespData->ubaChallenge, (UCHAR *)&stChallengeCode, sizeof(stChallengeCode));
	usDataLen += sizeof(ST_CHAP_DATA);
	memcpy(&szData[sizeof(ST_LNCP_HDR) + sizeof(ST_CHAP_DATA)], pszUser, unUserLen);
	usDataLen += unUserLen;

#if SUPPORT_PRINTF
	CHAR szChallenge[CHAP_CHALLENGE_LEN * 2 + 1]; 
	snprintf_hex(pstRespData->ubaChallenge, CHAP_CHALLENGE_LEN, szChallenge, sizeof(szChallenge) - 1, FALSE);
	printf("sent [Protocol CHAP, Id = %02X, Code = 'Response', Challenge = <%s>, name = \"%s\"]\r\n", pstHdr->ubIdentifier, szChallenge, pszUser); 
#endif

	//* 发送响应报文
	send_packet(pstcbPPP, PPP_CHAP, (UCHAR)RESPONSE, pstHdr->ubIdentifier, szData, (USHORT)usDataLen, TRUE, NULL); 
}

//* chap协议接收函数
void chap_recv(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
	PST_LNCP_HDR pstHdr = (PST_LNCP_HDR)pubPacket;

#if SUPPORT_PRINTF
	printf("recv [Protocol CHAP, Id = %02X, Code = '%s'", pstHdr->ubIdentifier, get_chap_code_name(pstHdr->ubCode));
#endif

	switch ((EN_CHAPCODE)pstHdr->ubCode)
	{
	case CHALLENGE: 
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
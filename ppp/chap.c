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
	CHAR szData[72];
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
	UINT unUserLen = strlen(pszUser);
	PST_CHAP_DATA pstRespData = (PST_CHAP_DATA)&szData[sizeof(ST_LNCP_HDR)]; 
	pstRespData->ubChallengeLen = CHAP_CHALLENGE_LEN; 
	memcpy(pstRespData->ubaChallenge, (UCHAR *)&stChallengeCode, sizeof(stChallengeCode));
	memcpy(&szData[sizeof(ST_LNCP_HDR) + sizeof(ST_CHAP_DATA)], pszUser, unUserLen);
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
	}
}

#endif
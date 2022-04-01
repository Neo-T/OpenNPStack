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

static void ppp_negotiate(PSTCB_NETIFPPP pstcbPPP, EN_ERROR_CODE *penErrCode)
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

	case AUTHENTICATE: 

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
			ppp_negotiate(pstcbPPP, penErrCode); 
			return; 
		}
	}
}

#endif
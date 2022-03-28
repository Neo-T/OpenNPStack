#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"

#if SUPPORT_PPP
#define SYMBOL_GLOBALS
#include "ppp/negotiation.h"
#undef SYMBOL_GLOBALS

void ppp_wait_ack_timeout(void *pvParam)
{

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
			start_negotiation(pstcbPPP->hTTY, pstcbPPP->pstNegoResult);
			break; 
		}
	}
}

#endif
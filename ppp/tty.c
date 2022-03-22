#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "ppp/chat.h"

#if SUPPORT_PPP
#define SYMBOL_GLOBALS
#include "ppp/tty.h"
#undef SYMBOL_GLOBALS

HTTY tty_init(const CHAR *pszTTYName, EN_ERROR_CODE *penErrCode)
{
	HTTY hTTY; 
	hTTY = os_open_tty(pszTTYName); 
	if (INVALID_HTTY == hTTY)
	{
		*penErrCode = ERROPENTTY;
		return INVALID_HTTY; 
	}

	return hTTY; 
}

BOOL tty_ready(HTTY hTTY, EN_ERROR_CODE *penErrCode)
{
	EN_ERROR_CODE enErrCode; 
	
	//* 先复位modem，以消除一切不必要的设备错误，如果不需要则port层只需实现一个无任何操作的空函数即可，或者直接注释掉这个函数的调用
	os_modem_reset(hTTY);
	do {
		if (!modem_ready(hTTY, &enErrCode))
			break; 

		if (!modem_dial(hTTY, &enErrCode))
			break; 

		return TRUE; 
	} while (FALSE);

	os_close_tty(hTTY);	
	return FALSE; 
}

#endif
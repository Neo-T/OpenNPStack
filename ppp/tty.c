#include "port/datatype.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "port/sys_config.h"
#include "errors.h"

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

BOOL tty_ready(HTTY hTTY)
{
	os_tty_reset(hTTY);


}

#endif
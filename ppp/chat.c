#include "port/datatype.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "port/sys_config.h"
#include "errors.h"

#if SUPPORT_PPP
#define SYMBOL_GLOBALS
#include "ppp/chat.h"
#undef SYMBOL_GLOBALS

BOOL modem_ready(HTTY hTTY)
{

}

#endif
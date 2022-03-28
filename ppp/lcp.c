#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"

#if SUPPORT_PPP
#include "ppp/negotiation.h"
#include "ppp/ppp.h"
#define SYMBOL_GLOBALS
#include "ppp/lcp.h"
#undef SYMBOL_GLOBALS

LCP_EXT BOOL start_negotiation(HTTY hTTY, PST_PPPNEGORESULT pstNegoResult, EN_ERROR_CODE *penErrCode)
{

}

#endif
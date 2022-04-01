#include "port/datatype.h"
#include "errors.h"
#include "utils.h"
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

void chap_recv(PSTCB_NETIFPPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen, EN_ERROR_CODE *penErrCode)
{

}

#endif
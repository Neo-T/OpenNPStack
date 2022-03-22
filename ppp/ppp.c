#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "ppp/tty.h"

#if SUPPORT_PPP
#include "ppp/ppp_frame.h"
#include "ppp/ppp_utils.h"
#define SYMBOL_GLOBALS
#include "ppp/ppp.h"
#undef SYMBOL_GLOBALS



#endif
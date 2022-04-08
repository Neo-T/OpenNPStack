/* onps.h
 *
 * open net protocol statck
 *
 * Neo-T, 创建于2022.03.21 10:19
 * 版本: 1.0
 *
 */
#ifndef ONPS_H
#define ONPS_H

#ifdef SYMBOL_GLOBALS
	#define ONPS_EXT
#else
	#define ONPS_EXT extern
#endif //* SYMBOL_GLOBALS
#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "mmu/buf_list.h"

#if SUPPORT_PPP
#include "ppp/negotiation.h"
#include "ppp/ppp.h"
#endif

ONPS_EXT BOOL open_npstack_load(EN_ERROR_CODE *penErrCode); 
ONPS_EXT void open_npstack_unload(void);

#endif

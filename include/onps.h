/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * open net protocol statck
 *
 * Neo-T, 创建于2022.03.21 10:19
 *
 * version 1.0.0.221017_RC
 */
#ifndef ONPS_H
#define ONPS_H

#ifdef SYMBOL_GLOBALS
	#define ONPS_EXT
#else
	#define ONPS_EXT extern
#endif //* SYMBOL_GLOBALS
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "one_shot_timer.h"
#include "netif/netif.h" 
#include "netif/route.h"
#if SUPPORT_PPP
#include "ppp/negotiation.h"
#include "ppp/ppp.h"
#endif
#if SUPPORT_ETHERNET
#include "ethernet/ethernet.h"
#include "ethernet/dhcp_client.h"
#endif
#include "ip/icmp.h"
#include "onps_input.h"
#include "bsd/socket.h"

ONPS_EXT BOOL open_npstack_load(EN_ONPSERR *penErr); 
ONPS_EXT void open_npstack_unload(void);

#endif

/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "one_shot_timer.h"
#include "mmu/buf_list.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "netif/netif.h"

#if SUPPORT_IPV6
#include "ip/ip.h"
#define SYMBOL_GLOBALS
#include "ethernet/dhcpv6.h"
#undef SYMBOL_GLOBALS
#include "ip/ipv6_configure.h"

#if SUPPORT_ETHERNET
INT dhcpv6_client_start(PST_IPv6_ROUTER pstRouter, EN_ONPSERR *penErr)
{

}
#endif
#endif

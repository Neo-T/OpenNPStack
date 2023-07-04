/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "one_shot_timer.h"
#include "onps_utils.h"
#include "protocols.h"
#include "onps_input.h"

#if NETTOOLS_TELNETSRV
#define SYMBOL_GLOBALS
#include "telnet/os_nvt.h"
#undef SYMBOL_GLOBALS

void os_nvt_init(void)
{
}

void os_nvt_uninit(void)
{
}

BOOL os_nvt_start(void *pvParam)
{

}

void os_nvt_stop(void *pvParam)
{

}

#if SUPPORT_ETHERNET
#if ETH_EXTRA_IP_EN
BOOL os_nvt_add_ip(const CHAR *pszIfName, in_addr_t unIp, in_addr_t unSubnetMask)
{
    return TRUE;
}

BOOL os_nvt_del_ip(const CHAR *pszIfName, in_addr_t unIp)
{
    return TRUE;
}
#endif //* #if ETH_EXTRA_IP_EN 
BOOL os_nvt_set_ip(const CHAR *pszIfName, in_addr_t unIp, in_addr_t unSubnetMask, in_addr_t unGateway)
{
    return TRUE; 
}
#endif //* #if SUPPORT_ETHERNET
#endif //* #if NETTOOLS_TELNETSRV

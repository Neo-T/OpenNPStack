/*
 * 作者：Neo-T，遵循Apache License 2.0开源许可协议
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

void os_nvt_stop(void *pvParam, BOOL blIsNvtEnd)
{

}

#if SUPPORT_ETHERNET && NVTCMD_IFIP_EN
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

BOOL os_nvt_set_mac(const CHAR *pszIfName, const CHAR *pszMac)
{
    return TRUE; 
}

BOOL os_nvt_set_dns(const CHAR *pszIfName, in_addr_t unPrimaryDns, in_addr_t unSecondaryDns)
{
    return TRUE; 
}

BOOL os_nvt_set_dhcp(const CHAR *pszIfName)
{
    return TRUE; 
}

void os_nvt_system_reset(void)
{

}
#endif //* #if SUPPORT_ETHERNET && NVTCMD_IFIP_EN

#if NVTCMD_ROUTE_EN
BOOL os_nvt_add_route_entry(const CHAR *pszIfName, in_addr_t unDestination, in_addr_t unGenmask, in_addr_t unGateway)
{
    return TRUE; 
}

BOOL os_nvt_del_route_entry(in_addr_t unDestination)
{
    return TRUE; 
}
#endif //* #if NVTCMD_ROUTE_EN

#if NETTOOLS_PING && NVTCMD_PING_EN
UINT os_get_elapsed_millisecs(void)
{
    return 0; 
}
#endif //* #if NETTOOLS_PING && NVTCMD_PING_EN

#if NETTOOLS_SNTP && NVTCMD_NTP_EN
void os_nvt_set_system_time(time_t tNtpTime)
{

}
#endif //* #if NETTOOLS_SNTP && NVTCMD_NTP_EN

#endif //* #if NETTOOLS_TELNETSRV

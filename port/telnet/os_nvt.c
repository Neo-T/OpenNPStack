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
#endif //* #if NETTOOLS_TELNETSRV
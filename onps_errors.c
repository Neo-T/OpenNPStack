#include "port/datatype.h"
#define SYMBOL_GLOBALS
#include "onps_errors.h"
#undef SYMBOL_GLOBALS
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"


static EN_ONPSERR l_enLastErr = ERRNO; 
static const ST_ONPSERR lr_staErrorList[] = {
	{ ERRNO, "no errors" },
	{ ERRNOPAGENODE, "no page nodes available"},
	{ ERRREQMEMTOOLARGE, "The requested memory is too large, please refer to the macro definition BUDDY_MEM_SIZE" },
	{ ERRNOFREEMEM, "The mmu has no memory available" }, 
	{ ERRMUTEXINITFAILED, "thread mutex initialization failed" }, 
	{ ERRNOBUFLISTNODE, "the buffer list node is empty" }, 
	{ ERRSEMINITFAILED, "thread semphore initialization failed" }, 
	{ ERROPENTTY, "tty open error" }, 
	{ ERRATWRITE, "write AT command error" }, 
	{ ERRATEXEC, "the at command returned an error" }, 
	{ ERRATEXECTIMEOUT, "AT command exec timeout" }, 
	{ ERRSIMCARD, "no sim card detected" }, 
	{ ERRREGMOBILENET, "Unable to register to mobile network" }, 
	{ ERRPPPDELIMITER, "ppp frame delimiter not found" }, 
	{ ERRTOOMANYTTYS, "too many ttys" }, 
	{ ERRTTYHANDLE, "invalid tty handle" }, 
	{ ERROSADAPTER, "os adaptation layer error" }, 
	{ ERRUNKNOWNPROTOCOL, "Unknown protocol type" }, 
	{ ERRPPPFCS, "ppp frame checksum error" }, 
	{ ERRNOIDLETIMER, "no idle timer" }, 
	{ ERRPPPWALISTNOINIT, "ppp's waiting list for ack is not initialized" }, 
    { ERRNONETIFNODE, "no netif nodes available" }, 
    { ERRUNSUPPIPPROTO, "Unsupported ip upper layer protocol" }, 
    { ERRUNSUPPIOPT, "Unsupported control options" }, 
    { ERRIPROTOMATCH, "Protocol match error" }, 
    { ERRNOROUTENODE, "no route nodes available" }, 
    { ERRSOCKETTYPE, "Unsupported socket type" }
}; 

const CHAR *onps_error(EN_ONPSERR enErr)
{
	UINT unIndex = (UINT)enErr;
	if (unIndex < sizeof(lr_staErrorList) / sizeof(ST_ONPSERR))
		return lr_staErrorList[unIndex].szDesc;

	return "unrecognized error code";
}

const CHAR *onps_get_last_error(EN_ONPSERR *penErr)
{
    EN_ONPSERR enLastErr;
    os_critical_init();
    os_enter_critical();
    enLastErr = l_enLastErr;
    os_exit_critical();

    if (penErr)
        *penErr = enLastErr;

    return onps_error(enLastErr);
}

void onps_set_last_error(EN_ONPSERR enErr)
{
    os_critical_init();
    os_enter_critical();
    l_enLastErr = enErr;
    os_exit_critical();
}

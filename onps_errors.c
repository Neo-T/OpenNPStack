#include "port/datatype.h"

#define SYMBOL_GLOBALS
#include "onps_errors.h"
#undef SYMBOL_GLOBALS

static const ST_ONPSERR lr_staErrorList[] = {
	{ ERRNO, "no error" },
	{ ERRNOPAGENODE, "no page nodes available"},
	{ ERRREQMEMTOOLARGE, "the requested memory is too large, please refer to the macro definition BUDDY_MEM_SIZE" },
	{ ERRNOFREEMEM, "the mmu has no memory available" }, 
	{ ERRMUTEXINITFAILED, "thread mutex initialization failed" }, 
	{ ERRNOBUFLISTNODE, "the buffer list node is empty" }, 
	{ ERRSEMINITFAILED, "thread semphore initialization failed" }, 
	{ ERROPENTTY, "tty open error" }, 
	{ ERRATWRITE, "write AT command error" }, 
	{ ERRATEXEC, "the at command returned an error" }, 
	{ ERRATEXECTIMEOUT, "at command timeout" }, 
	{ ERRSIMCARD, "no sim card detected" }, 
	{ ERRREGMOBILENET, "Unable to register to mobile network" }, 
	{ ERRPPPDELIMITER, "ppp frame delimiter not found" }, 
	{ ERRTOOMANYTTYS, "Too many ttys" }, 
	{ ERRTTYHANDLE, "invalid tty handle" }, 
	{ ERROSADAPTER, "os adaptation layer error" }, 
	{ ERRUNKNOWNPROTOCOL, "Unknown protocol type" }, 
	{ ERRPPPFCS, "ppp frame checksum error" }, 
	{ ERRNOIDLETIMER, "no idle timer" }, 
	{ ERRPPPWALISTNOINIT, "ppp's waiting list for ack is not initialized" }, 
    { ERRNONETIFNODE, "no netif nodes available" }, 
    { ERRUNSUPPIPPROTO, "unsupported ip upper layer protocol" }, 
    { ERRUNSUPPIOPT, "Unsupported control options" }, 
    { ERRIPROTOMATCH, "Protocol match error" }, 
    { ERRNOROUTENODE, "no route nodes available" }
}; 

const CHAR *onps_error(EN_ONPSERR enErr)
{
	UINT unIndex = (UINT)enErr;
	if (unIndex < sizeof(lr_staErrorList) / sizeof(ST_ONPSERR))
		return lr_staErrorList[unIndex].szDesc;

	return "unrecognized error code";
}

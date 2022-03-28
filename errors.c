#include "port/datatype.h"
#include "port/os_datatype.h"

#define SYMBOL_GLOBALS
#include "errors.h"
#undef SYMBOL_GLOBALS

static const ST_ERROR lr_staErrorList[] = {
	{ ERRNO, "no error" },
	{ ERRNOPAGENODES, "no page nodes available"},
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
	{ ERRNOIDLETIMER, "no idle timer" }
}; 

const CHAR *error(EN_ERROR_CODE enErrCode)
{
	UINT unIndex = (UINT)enErrCode;
	if (unIndex < sizeof(lr_staErrorList) / sizeof(ST_ERROR))
		return lr_staErrorList[unIndex].szDesc;

	return "unrecognized error code";
}
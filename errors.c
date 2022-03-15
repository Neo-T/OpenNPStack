#include "port/datatype.h"
#include "port/os_datatype.h"
#include "port/sys_config.h"

#define SYMBOL_GLOBALS
#include "errors.h"
#undef SYMBOL_GLOBALS

static const ST_ERROR lr_staErrorList[] = {
	{ ERRNO, "no error" },
	{ ERRNOPAGENODES, "no page nodes available"},
	{ ERRREQMEMTOOLARGE, "the requested memory is too large, please refer to the macro definition BUDDY_MEM_SIZE" },
	{ ERRNOFREEMEM, "the mmu has no memory available" }, 
	{ ERRMUTEXINITFAILED, "thread mutex initialization failed" },
}; 

const CHAR *error(EN_ERROR_CODE enErrCode)
{
	UINT unIndex = (UINT)enErrCode;
	if (unIndex < sizeof(lr_staErrorList) / sizeof(ST_ERROR))
		return lr_staErrorList[unIndex].szDesc;

	return "unrecognized error code";
}
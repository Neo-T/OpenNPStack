#include "port/datatype.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "port/sys_config.h"
#include "errors.h"

#define SYMBOL_GLOBALS
#include "timer.h"
#undef SYMBOL_GLOBALS

static HSEM l_hSemTimeout = INVALID_HSEM;

BOOL pstack_timer_init(EN_ERROR_CODE *penErrCode)
{
	l_hSemTimeout = os_thread_sem_init(0);
	if (INVALID_HSEM != l_hSemTimeout)
		return TRUE; 

	*penErrCode = ERRSEMINITFAILED; 
	return FALSE; 
}

void THTimerCount(void *pvParam)
{
	while (TRUE)
	{

	}
}

void THTimeoutHandler(void *pvParam)
{

}


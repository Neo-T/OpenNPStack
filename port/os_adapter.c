#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "one_shot_timer.h"

#if SUPPORT_PPP
#include "protocols.h"
#include "ppp/ppp.h"
#endif

#define SYMBOL_GLOBALS
#include "port/os_adapter.h"
#undef SYMBOL_GLOBALS

//* 协议栈内部工作线程列表
const static STCB_PSTACKTHREAD lr_stcbaPStackThread[] = {
	{ thread_one_shot_timer_count, NULL},
	{ thread_one_shot_timeout_handler, NULL }, 
#if SUPPORT_PPP
	//* 在此按照顺序建立ppp工作线程，入口函数为thread_ppp_handler()，线程入口参数为该ppp链路在l_staTTY数组的存储单元索引值
	//* 其直接强行进行数据类型转换即可，即作为线程入口参数时直接以如下形式传递：
	//* (void *)nPPPIdx
	//* 不要传递参数地址，即(void *)&nPPPIdx，这种方式是错误的
#endif
}; 

/* 用户自定义变量声明区 */
/* …… */

//* 当前线程休眠指定的秒数，参数unSecs指定要休眠的秒数
void os_sleep_secs(UINT unSecs)
{
	/* 用户自定义代码 */
	/* …… */
}

//* 获取系统启动以来已运行的秒数（从0开始）
UINT os_get_system_secs(void)
{
	/* 用户自定义代码 */
	/* …… */

	return 0; 
}

void os_thread_pstack_start(void *pvParam)
{
	/* 用户自定义代码 */
	/* …… */

	//* 建立工作线程
	INT i; 
	for (i = 0; i < sizeof(lr_stcbaPStackThread) / sizeof(STCB_PSTACKTHREAD); i++)
	{
		//* 在此按照顺序建立工作线程
	}

	/* 用户自定义代码 */
	/* …… */
}

HMUTEX os_thread_mutex_init(void)
{
	/* 用户自定义代码 */
	/* …… */
	return INVALID_HMUTEX; //* 初始失败要返回一个无效句柄
}

void os_thread_mutex_lock(HMUTEX hMutex)
{
	/* 用户自定义代码 */
	/* …… */
}

void os_thread_mutex_unlock(HMUTEX hMutex)
{
	/* 用户自定义代码 */
	/* …… */
}

void os_thread_mutex_uninit(HMUTEX hMutex)
{
	/* 用户自定义代码 */
	/* …… */
}

HSEM os_thread_sem_init(UINT unInitVal, UINT unCount)
{
	/* 用户自定义代码 */
	/* …… */

	return INVALID_HSEM; //* 初始失败要返回一个无效句柄
}

void os_thread_sem_post(HSEM hSem)
{
	/* 用户自定义代码 */
	/* …… */
}

INT os_thread_sem_pend(HSEM hSem, UINT unWaitSecs)
{
	/* 用户自定义代码 */
	/* …… */

	return -1; 
}

#if SUPPORT_PPP
HTTY os_open_tty(const CHAR *pszTTYName)
{
	/* 用户自定义代码 */
	/* …… */

	return INVALID_HTTY; 
}

void os_close_tty(HTTY hTTY)
{
	/* 用户自定义代码 */
	/* …… */
}

INT os_tty_send(HTTY hTTY, UCHAR *pubData, INT nDataLen)
{
	/* 用户自定义代码 */
	/* …… */

	return 0; 
}

INT os_tty_recv(HTTY hTTY, UCHAR *pubRcvBuf, INT nRcvBufLen)
{
	/* 用户自定义代码 */
	/* …… */

	return 0;
}

void os_modem_reset(HTTY hTTY)
{
	/* 用户自定义代码，不需要复位modem设备则这里可以不进行任何操作 */
	/* …… */
}
#endif
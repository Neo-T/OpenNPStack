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

#if SUPPORT_PPP
#include "ppp/negotiation_storage.h"
#include "ppp/ppp.h"
#endif

#include "ip/tcp.h"

#define SYMBOL_GLOBALS
#include "port/os_adapter.h"
#undef SYMBOL_GLOBALS

#if SUPPORT_PPP
//* 在此指定连接modem的串行口，以此作为tty终端进行ppp通讯，其存储索引应与os_open_tty()返回的tty句柄值一一对应
const CHAR *or_pszaTTY[PPP_NETLINK_NUM] = { "SCP3" };
const ST_DIAL_AUTH_INFO or_staDialAuth[PPP_NETLINK_NUM] = {
    { "4gnet", "card", "any_char" },  /* 注意ppp账户和密码尽量控制在20个字节以内，太长需要需要修改chap.c中send_response()函数的szData数组容量及 */
                                      /* pap.c中pap_send_auth_request()函数的ubaPacket数组的容量，确保其能够封装一个完整的响应报文              */
};
ST_PPPNEGORESULT o_staNegoResult[PPP_NETLINK_NUM] = {
    {
        { 0, PPP_MRU, ACCM_INIT,{ PPP_CHAP, 0x05 /* 对于CHAP协议来说，0-4未使用，0x05代表采用MD5算法 */ }, TRUE, TRUE, FALSE, FALSE },
        { IP_ADDR_INIT, DNS_ADDR_INIT, DNS_ADDR_INIT, IP_ADDR_INIT, MASK_INIT }, 0
    },

    /* 系统存在几路ppp链路，就在这里添加几路的协商初始值，如果不确定，可以直接将上面预定义的初始值直接复制过来即可 */
};
#endif

#if SUPPORT_ETHERNET
const CHAR *or_pszaEthName[ETHERNET_NUM] = {
    "eth0"
};
#endif

//* 协议栈内部工作线程列表
const static STCB_PSTACKTHREAD lr_stcbaPStackThread[] = {
	{ thread_one_shot_timer_count, NULL}, 	
#if SUPPORT_PPP
	//* 在此按照顺序建立ppp工作线程，入口函数为thread_ppp_handler()，线程入口参数为os_open_tty()返回的tty句柄值
	//* 其直接强行进行数据类型转换即可，即作为线程入口参数时直接以如下形式传递：
	//* (void *)nPPPIdx
	//* 不要传递参数地址，即(void *)&nPPPIdx，这种方式是错误的
#endif

#if SUPPORT_SACK
    { thread_tcp_handler, NULL },
#endif
}; 

/* 用户自定义变量声明区 */
/* …… */

//* 当前线程休眠指定的秒数，参数unSecs指定要休眠的秒数
void os_sleep_secs(UINT unSecs)
{
#error os_sleep_secs() cannot be empty
}

//* 当前线程休眠指定的毫秒数，单位：毫秒 
void os_sleep_ms(UINT unMSecs)
{
#error os_sleep_ms() cannot be empty
}

//* 获取系统启动以来已运行的秒数（从0开始）
UINT os_get_system_secs(void)
{
#error os_get_system_secs() cannot be empty

	return 0; 
}

//* 获取系统启动以来已运行的毫秒数（从0开始）
UINT os_get_system_msecs(void)
{
#error os_get_system_msecs() cannot be empty

    return 0;
}

void os_thread_onpstack_start(void *pvParam)
{
#error os_thread_onpstack_start() cannot be empty

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
#error os_thread_mutex_init() cannot be empty

	return INVALID_HMUTEX; //* 初始失败要返回一个无效句柄
}

void os_thread_mutex_lock(HMUTEX hMutex)
{
#error os_thread_mutex_lock() cannot be empty
}

void os_thread_mutex_unlock(HMUTEX hMutex)
{
#error os_thread_mutex_unlock() cannot be empty
}

void os_thread_mutex_uninit(HMUTEX hMutex)
{
#error os_thread_mutex_uninit() cannot be empty
}

HSEM os_thread_sem_init(UINT unInitVal, UINT unCount)
{
#error os_thread_sem_init() cannot be empty

	return INVALID_HSEM; //* 初始失败要返回一个无效句柄
}

void os_thread_sem_post(HSEM hSem)
{
#error os_thread_sem_post() cannot be empty
}

INT os_thread_sem_pend(HSEM hSem, INT nWaitSecs)
{
#error os_thread_sem_pend() cannot be empty

	return -1; 
}

void os_thread_sem_uninit(HSEM hSem)
{
#error os_thread_sem_uninit() cannot be empty
}

#if SUPPORT_PPP
HTTY os_open_tty(const CHAR *pszTTYName)
{
#error os_open_tty() cannot be empty

	return INVALID_HTTY; 
}

void os_close_tty(HTTY hTTY)
{
#error os_close_tty() cannot be empty
}

INT os_tty_send(HTTY hTTY, UCHAR *pubData, INT nDataLen)
{
#error os_tty_send() cannot be empty

	return 0; 
}

INT os_tty_recv(HTTY hTTY, UCHAR *pubRcvBuf, INT nRcvBufLen, INT nWaitSecs)
{
#error os_tty_recv() cannot be empty

	return 0;
}

void os_modem_reset(HTTY hTTY)
{
	/* 用户自定义代码，不需要复位modem设备则这里可以不进行任何操作 */
	/* …… */
}
#endif


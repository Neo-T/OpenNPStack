#include "port/datatype.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "port/sys_config.h"
#include "errors.h"

#define SYMBOL_GLOBALS
#include "timer.h"
#undef SYMBOL_GLOBALS

static HMUTEX l_hMtxFreeOneShotTimer;	//* 可用定时器链表同步锁
static HMUTEX l_hMtxOneShotTimer;		//* 正在计时的定时器链表同步锁
static HMUTEX l_hMtxOneShotTimeout;		//* 已经超时溢出的定时器链表同步锁
static HSEM l_hSemOneShotTimeout = INVALID_HSEM;
static ST_ONESHOTTIMER l_staOneShotTimerNode[TIMER_NUM];
static PST_ONESHOTTIMER l_pstFreeOneShotTimerLink;
static PST_ONESHOTTIMER l_pstOneShotTimerLink = NULL;
static PST_ONESHOTTIMER l_pstOneShotTimeoutLink = NULL;

//* 定时器初始化
BOOL pstack_timer_init(EN_ERROR_CODE *penErrCode)
{
	INT i; 
	for (i = 0; i < TIMER_NUM - 1; i++) //* 将定时器链表链接起来
	{
		l_staOneShotTimerNode[i].pstNext = &l_staOneShotTimerNode[i + 1];
	}
	l_staOneShotTimerNode[TIMER_NUM - 1].pstNext = NULL; //* 最后一个节点单独赋值
	l_pstFreeOneShotTimerLink = &l_staOneShotTimerNode[0];		  //* 接入链表头，形成真正的链表

	do {
		//* 定时器队列某个节点超时溢出时将发送该信号，所以需要在这里建立该信号
		l_hSemOneShotTimeout = os_thread_sem_init(0);
		if (INVALID_HSEM == l_hSemOneShotTimeout)
		{
			*penErrCode = ERRSEMINITFAILED;
			break; 
		}

		//* 建立可用定时器队列同步锁
		l_hMtxFreeOneShotTimer = os_thread_mutex_init();
		if (INVALID_HMUTEX == l_hMtxFreeOneShotTimer)
		{
			*penErrCode = ERRMUTEXINITFAILED; 
			break; 
		}

		//* 建立已开启计时的定时器队列同步锁
		l_hMtxOneShotTimer = os_thread_mutex_init();
		if (INVALID_HMUTEX == l_hMtxOneShotTimer)
		{
			*penErrCode = ERRMUTEXINITFAILED;
			break;
		}

		//* 建立已计时溢出的定时器队列同步锁
		l_hMtxOneShotTimeout = os_thread_mutex_init();
		if (INVALID_HMUTEX == l_hMtxOneShotTimeout)
		{
			*penErrCode = ERRMUTEXINITFAILED;
			break;
		}

		return TRUE; 
	} while (FALSE); 

	if (INVALID_HSEM != l_hSemOneShotTimeout)
		os_thread_sem_uninit(l_hSemOneShotTimeout);

	if (INVALID_HMUTEX != l_hMtxFreeOneShotTimer)
		os_thread_mutex_uninit(l_hMtxFreeOneShotTimer);

	if (INVALID_HMUTEX != l_hMtxOneShotTimeout)
		os_thread_mutex_uninit(l_hMtxOneShotTimeout);
	
	return FALSE; 
}

void pstack_thread_timer_count(void *pvParam)
{
	PST_ONESHOTTIMER pstTimer; 
	while (TRUE)
	{
		pstTimer = l_pstOneShotTimerLink; 
		while()

		os_sleep_secs(1);
	}
}

void pstack_thread_timeout_handler(void *pvParam)
{
	while (TRUE)
	{

	}
}

//* 分配一个新的one-shot定时器
PST_ONESHOTTIMER pstack_one_shot_timer_new(INT nTimeoutCount, void(*pfunTimeoutHandler)(void *pvParam), void *pvParam)
{
	PST_ONESHOTTIMER pstTimer = NULL;

	//* 从可用队列中摘取一个空闲节点
	os_thread_mutex_lock(l_hMtxFreeOneShotTimer);
	{
		pstTimer = l_pstFreeOneShotTimerLink;
		if (l_pstFreeOneShotTimerLink)
			l_pstFreeOneShotTimerLink = l_pstFreeOneShotTimerLink->pstNext;
	}
	os_thread_mutex_unlock(l_hMtxFreeOneShotTimer);

	//* 存在空闲节点则赋值并挂接到计时队列中
	if (pstTimer)
	{
		//* 先赋值再挂载，否则可能导致计数线程出现错误
		pstTimer->pfunTimeoutHandler = pfunTimeoutHandler;
		pstTimer->pvParam = pvParam;
		pstTimer->nTimeoutCount = nTimeoutCount;

		//* 挂接到计时队列中，开始计数
		os_thread_mutex_lock(l_hMtxOneShotTimer);
		{
			pstTimer->pstNext = l_pstOneShotTimerLink;
			l_pstOneShotTimerLink = pstTimer;
		}
		os_thread_mutex_unlock(l_hMtxOneShotTimer);
	}

	return pstTimer; 
}

//* 释放占用的定时器资源
void pstack_one_shot_timer_free(PST_ONESHOTTIMER pstTimer)
{
	os_thread_mutex_lock(l_hMtxFreeOneShotTimer);
	{				
		pstTimer->pstNext = l_pstFreeOneShotTimerLink; 					
		l_pstFreeOneShotTimerLink = pstTimer;
	}
	os_thread_mutex_unlock(l_hMtxFreeOneShotTimer);
}




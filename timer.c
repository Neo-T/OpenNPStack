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
BOOL timer_init(EN_ERROR_CODE *penErrCode)
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

//* 这并不是一个精确的定时器计时队列，这依赖于休眠精度以及队列长度，但对于我们的应用场景来说已经足够使用
void thread_timer_count(void *pvParam)
{
	PST_ONESHOTTIMER pstTimer, pstPrevTimer; 
	while (TRUE)
	{
		os_thread_mutex_lock(l_hMtxOneShotTimer);
		{
			pstTimer = l_pstOneShotTimerLink;
			pstPrevTimer = NULL;
			while (pstTimer)
			{
				pstTimer->nTimeoutCount--;
				if (pstTimer->nTimeoutCount <= 0)
				{
					//* 先从计时器队列摘除
					if (pstPrevTimer)
						pstPrevTimer->pstNext = pstTimer->pstNext;
					else
						l_pstOneShotTimerLink = pstTimer->pstNext;

					//* 指向下一个节点
					pstTimer = pstTimer->pstNext;

					//* 最后添加到超时溢出队列
					os_thread_mutex_lock(l_hMtxOneShotTimeout);
					{
						pstTimer->pstNext = l_pstOneShotTimeoutLink;
						l_pstOneShotTimeoutLink = pstTimer;
					}
					os_thread_mutex_unlock(l_hMtxOneShotTimeout);

					//* 通知计时溢出线程处理溢出事件
					os_thread_sem_post(l_hSemOneShotTimeout);
				}
				else //* 计时尚未结束，检查下一个节点
				{
					pstPrevTimer = pstTimer;
					pstTimer = pstTimer->pstNext;
				}
			}
		}
		os_thread_mutex_unlock(l_hMtxOneShotTimer);		

		//* 这个休眠可以不用特别精确（1秒左右），我们的应用场景足够了
		os_sleep_secs(1);
	}
}

//* 计时溢出事件处理线程
void thread_timeout_handler(void *pvParam)
{
	PST_ONESHOTTIMER pstTimer;
	while (TRUE)
	{
		//* 等待溢出事件到达
		os_thread_sem_pend(l_hSemOneShotTimeout, 0);

		//* 取出已经溢出的定时器
		os_thread_mutex_lock(l_hMtxOneShotTimeout);
		{
			pstTimer = l_pstOneShotTimeoutLink; 			
			if (pstTimer)
				l_pstOneShotTimeoutLink = pstTimer->pstNext; 
		}
		os_thread_mutex_unlock(l_hMtxOneShotTimeout);

		//* 执行溢出事件处理函数
		if (pstTimer)
		{
			//* 执行溢出函数
			pstTimer->pfunTimeoutHandler(pstTimer, pstTimer->pvParam);
		}
	}
}

//* 分配一个新的one-shot定时器
PST_ONESHOTTIMER one_shot_timer_new(INT nTimeoutCount, void(*pfunTimeoutHandler)(void *pvParam), void *pvParam)
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

void one_short_timer_recount(PST_ONESHOTTIMER pstTimer, INT nTimeoutCount)
{
	PST_ONESHOTTIMER pstNextTimer;

	//*	确保计时队列中还存在这个节点，否则没必要重计数了
	os_thread_mutex_lock(l_hMtxOneShotTimer);
	{
		pstNextTimer = l_pstOneShotTimerLink;
		while (pstNextTimer)
		{
			if (pstTimer == pstNextTimer)
			{
				pstTimer->nTimeoutCount = nTimeoutCount; 
				break; 
			}

			pstNextTimer = pstNextTimer->pstNext; 
		}
	}
	os_thread_mutex_unlock(l_hMtxOneShotTimer); 
}

void one_short_timer_stop(PST_ONESHOTTIMER pstTimer)
{
	PST_ONESHOTTIMER pstNextTimer;

	//*	确保计时队列中还存在这个节点，否则没必要重计数了
	os_thread_mutex_lock(l_hMtxOneShotTimer);
	{
		pstNextTimer = l_pstOneShotTimerLink;
		while (pstNextTimer)
		{
			if (pstTimer == pstNextTimer)
			{
				
				break;
			}

			pstNextTimer = pstNextTimer->pstNext;
		}
	}
	os_thread_mutex_unlock(l_hMtxOneShotTimer);
}

//* 释放占用的定时器资源
void one_shot_timer_free(PST_ONESHOTTIMER pstTimer)
{
	os_thread_mutex_lock(l_hMtxFreeOneShotTimer);
	{				
		pstTimer->pstNext = l_pstFreeOneShotTimerLink; 					
		l_pstFreeOneShotTimerLink = pstTimer;
	}
	os_thread_mutex_unlock(l_hMtxFreeOneShotTimer);
}




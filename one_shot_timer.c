#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"

#define SYMBOL_GLOBALS
#include "one_shot_timer.h"
#undef SYMBOL_GLOBALS

static HMUTEX l_hMtxFreeOneShotTimer;	//* 可用定时器链表同步锁
static HMUTEX l_hMtxOneShotTimer;		//* 正在计时的定时器链表同步锁
static HMUTEX l_hMtxOneShotTimeout;		//* 已经超时溢出的定时器链表同步锁
static HSEM l_hSemOneShotTimeout = INVALID_HSEM;
static ST_ONESHOTTIMER l_staOneShotTimerNode[ONE_SHOT_TIMER_NUM];
static PST_ONESHOTTIMER l_pstFreeOneShotTimerLink;
static PST_ONESHOTTIMER l_pstOneShotTimerLink = NULL;
static PST_ONESHOTTIMER l_pstOneShotTimeoutLink = NULL;
static BOOL l_blIsRunning = TRUE; 
static UCHAR l_ubaThreadExitFlag[2] = { TRUE, TRUE }; 

//* 定时器初始化（栈开始工作前必须要先调用这个函数进行定时器初始化）
BOOL one_shot_timer_init(EN_ERROR_CODE *penErrCode)
{
	INT i; 
	for (i = 0; i < ONE_SHOT_TIMER_NUM - 1; i++) //* 将定时器链表链接起来
	{
		l_staOneShotTimerNode[i].pstNext = &l_staOneShotTimerNode[i + 1];
	}
	l_staOneShotTimerNode[ONE_SHOT_TIMER_NUM - 1].pstNext = NULL; //* 最后一个节点单独赋值
	l_pstFreeOneShotTimerLink = &l_staOneShotTimerNode[0];		  //* 接入链表头，形成真正的链表

	do {
		//* 定时器队列某个节点超时溢出时将发送该信号，所以需要在这里建立该信号
		l_hSemOneShotTimeout = os_thread_sem_init(0, ONE_SHOT_TIMER_NUM + 1);
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

	if (INVALID_HMUTEX != l_hMtxOneShotTimer)
		os_thread_mutex_uninit(l_hMtxOneShotTimer);
	
	return FALSE; 
}

//* 定时器去初始化
void one_shot_timer_uninit(void)
{
    l_blIsRunning = FALSE; 

    while (!l_ubaThreadExitFlag[0])
        os_sleep_secs(1);

    while (!l_ubaThreadExitFlag[1])
        os_sleep_secs(1);

    if (INVALID_HSEM != l_hSemOneShotTimeout)
        os_thread_sem_uninit(l_hSemOneShotTimeout);

    if (INVALID_HMUTEX != l_hMtxFreeOneShotTimer)
        os_thread_mutex_uninit(l_hMtxFreeOneShotTimer);

    if (INVALID_HMUTEX != l_hMtxOneShotTimer)
        os_thread_mutex_uninit(l_hMtxOneShotTimer);

    if (INVALID_HMUTEX != l_hMtxOneShotTimeout)
        os_thread_mutex_uninit(l_hMtxOneShotTimeout);
}

//* 结束两个定时器线程，并释放所有工作队列，并归还给系统
void one_shot_timer_stop(void)
{
	l_blIsRunning = FALSE;
}

//* 这并不是一个精确的定时器计时队列，这依赖于休眠精度以及队列长度，但对于我们的应用场景来说已经足够使用
void thread_one_shot_timer_count(void *pvParam)
{
	PST_ONESHOTTIMER pstTimer, pstPrevTimer, pstNextTimer; 

    l_ubaThreadExitFlag[0] = FALSE;
	while (l_blIsRunning)
	{
		os_thread_mutex_lock(l_hMtxOneShotTimer);
		{
			pstNextTimer = l_pstOneShotTimerLink;
			pstPrevTimer = NULL;
			while (pstNextTimer)
			{
				if (pstNextTimer->nTimeoutCount-- <= 0)
				{
					//* 先从计时器队列摘除
					if (pstPrevTimer)
						pstPrevTimer->pstNext = pstNextTimer->pstNext; 
					else
						l_pstOneShotTimerLink = pstNextTimer->pstNext; 					
					pstTimer = pstNextTimer;

					//* 指向下一个节点
					pstNextTimer = pstNextTimer->pstNext;

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
					pstPrevTimer = pstNextTimer;
					pstNextTimer = pstNextTimer->pstNext;
				}
			}
		}
		os_thread_mutex_unlock(l_hMtxOneShotTimer);		

		//* 这个休眠可以不用特别精确（1秒左右），我们的应用场景足够了
		os_sleep_secs(1);
	}

	//* 回收资源
	os_thread_mutex_lock(l_hMtxOneShotTimer);
	{
		pstNextTimer = l_pstOneShotTimerLink;
		while (pstNextTimer)
		{
			//* 先从计时器队列摘除
			pstTimer = pstNextTimer;
			pstNextTimer = pstNextTimer->pstNext;

			//* 归还
			one_shot_timer_free(pstTimer);
		}
	}
	os_thread_mutex_unlock(l_hMtxOneShotTimer);

    l_ubaThreadExitFlag[0] = TRUE;
}

//* 计时溢出事件处理线程，当用户的计时器溢出函数执行后，当前定时器将被线程自动释放并归还给系统，用户无需执行释放操作
void thread_one_shot_timeout_handler(void *pvParam)
{
	PST_ONESHOTTIMER pstTimer, pstNextTimer;

    l_ubaThreadExitFlag[1] = FALSE;
	while (l_blIsRunning)
	{
		//* 等待溢出事件到达
		if (os_thread_sem_pend(l_hSemOneShotTimeout, 1) < 0)
			continue; 

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
			pstTimer->pfunTimeoutHandler(pstTimer->pvParam);
			//* 归还给系统
			one_shot_timer_free(pstTimer); 
		}
	}

	//* 回收资源
	os_thread_mutex_lock(l_hMtxOneShotTimeout);
	{
		pstNextTimer = l_pstOneShotTimeoutLink;
		while (pstNextTimer)
		{
			//* 先从队列摘除
			pstTimer = pstNextTimer;
			pstNextTimer = pstNextTimer->pstNext;

			//* 归还
			one_shot_timer_free(pstTimer);
		}
	}
	os_thread_mutex_unlock(l_hMtxOneShotTimeout);

    l_ubaThreadExitFlag[1] = TRUE;
}

//* 分配一个新的one-shot定时器
PST_ONESHOTTIMER one_shot_timer_new(PFUN_ONESHOTTIMEOUT_HANDLER pfunTimeoutHandler, void *pvParam, INT nTimeoutCount)
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

void one_shot_timer_recount(PST_ONESHOTTIMER pstTimer, INT nTimeoutCount)
{
	PST_ONESHOTTIMER pstNextTimer;

	//* 必须大于0才可
	if (nTimeoutCount <= 0)
		return; 

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

//* 这个函数的目的是安全停止计时器并将其归还给系统，不再占用，与one_shot_timer_free()函数不同
//* ，该函数需要先判断其是否依然还在计数，是，则停止并归还给系统，否则不做任何处理
void one_shot_timer_safe_free(PST_ONESHOTTIMER pstTimer)
{
	PST_ONESHOTTIMER pstNextTimer, pstPrevTimer;
	BOOL blIsExist = FALSE; 

	//*	确保计时队列中还存在这个节点，否则不做任何处理
	os_thread_mutex_lock(l_hMtxOneShotTimer);
	{
		pstNextTimer = l_pstOneShotTimerLink;
		pstPrevTimer = NULL; 
		while (pstNextTimer)
		{
			if (pstTimer == pstNextTimer) //* 存在这个定时器，从计时器队列摘除之
			{
				if (pstPrevTimer)
					pstPrevTimer->pstNext = pstTimer->pstNext;
				else
					l_pstOneShotTimerLink = pstTimer->pstNext;

				blIsExist = TRUE;
				
				break;
			}

			pstPrevTimer = pstNextTimer; 
			pstNextTimer = pstNextTimer->pstNext;
		}
	}
	os_thread_mutex_unlock(l_hMtxOneShotTimer);

	//* 存在则归还给系统（这里未使用函数调用的方式以减少入栈出栈带来的内存及性能损耗）
	if (blIsExist)
	{
		os_thread_mutex_lock(l_hMtxFreeOneShotTimer);
		{
			pstTimer->pstNext = l_pstFreeOneShotTimerLink;
			l_pstFreeOneShotTimerLink = pstTimer;
		}
		os_thread_mutex_unlock(l_hMtxFreeOneShotTimer);
	}
}

//* 释放占用的定时器资源，不做任何判断直接释放并归还给系统
void one_shot_timer_free(PST_ONESHOTTIMER pstTimer)
{
	os_thread_mutex_lock(l_hMtxFreeOneShotTimer);
	{				
		pstTimer->pstNext = l_pstFreeOneShotTimerLink; 					
		l_pstFreeOneShotTimerLink = pstTimer;
	}
	os_thread_mutex_unlock(l_hMtxFreeOneShotTimer);
}




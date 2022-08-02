#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"

#define SYMBOL_GLOBALS
#include "one_shot_timer.h"
#undef SYMBOL_GLOBALS

typedef struct _ST_ONESHOTTIMER_ { //* 定时器
    PST_ONESHOTTIMER pstNext;
    PFUN_ONESHOTTIMEOUT_HANDLER pfunTimeoutHandler;
    void *pvParam;
    INT nTimeoutCount;	//* 溢出值，单位：秒	
} ST_ONESHOTTIMER, *PST_ONESHOTTIMER; 

//static HMUTEX l_hMtxFreeOneShotTimer;	//* 可用定时器链表同步锁
//static HMUTEX l_hMtxOneShotTimer;		//* 正在计时的定时器链表同步锁
static ST_ONESHOTTIMER l_staOneShotTimerNode[ONE_SHOT_TIMER_NUM];
static PST_ONESHOTTIMER l_pstFreeOneShotTimerLink;
static PST_ONESHOTTIMER l_pstOneShotTimerLink = NULL;
static BOOL l_blIsRunning = TRUE; 
static UCHAR l_ubThreadExitFlag = TRUE; 

//* 定时器初始化（栈开始工作前必须要先调用这个函数进行定时器初始化）
BOOL one_shot_timer_init(EN_ONPSERR *penErr)
{
	INT i; 
	for (i = 0; i < ONE_SHOT_TIMER_NUM - 1; i++) //* 将定时器链表链接起来
	{
		l_staOneShotTimerNode[i].pstNext = &l_staOneShotTimerNode[i + 1];
	}
	l_staOneShotTimerNode[ONE_SHOT_TIMER_NUM - 1].pstNext = NULL; //* 最后一个节点单独赋值
	l_pstFreeOneShotTimerLink = &l_staOneShotTimerNode[0];		  //* 接入链表头，形成真正的链表

	do {
    #if 0
		//* 建立可用定时器队列同步锁
		l_hMtxFreeOneShotTimer = os_thread_mutex_init();
		if (INVALID_HMUTEX == l_hMtxFreeOneShotTimer)
		{
			*penErr = ERRMUTEXINITFAILED; 
			break; 
		}

		//* 建立已开启计时的定时器队列同步锁
		l_hMtxOneShotTimer = os_thread_mutex_init();
		if (INVALID_HMUTEX == l_hMtxOneShotTimer)
		{
			*penErr = ERRMUTEXINITFAILED;
			break;
		}		
    #endif

		return TRUE; 
	} while (FALSE); 	

	//if (INVALID_HMUTEX != l_hMtxFreeOneShotTimer)
	//	os_thread_mutex_uninit(l_hMtxFreeOneShotTimer);	
	
	return FALSE; 
}

//* 定时器去初始化
void one_shot_timer_uninit(void)
{
    l_blIsRunning = FALSE; 

    while (!l_ubThreadExitFlag)
        os_sleep_secs(1);     

    //if (INVALID_HMUTEX != l_hMtxFreeOneShotTimer)
    //    os_thread_mutex_uninit(l_hMtxFreeOneShotTimer);

    //if (INVALID_HMUTEX != l_hMtxOneShotTimer)
    //    os_thread_mutex_uninit(l_hMtxOneShotTimer);    
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

    os_critical_init();

    l_ubThreadExitFlag = FALSE;
	while (l_blIsRunning)
	{
		//os_thread_mutex_lock(l_hMtxOneShotTimer);
        os_enter_critical();
		{
			pstNextTimer = l_pstOneShotTimerLink;
			pstPrevTimer = NULL;
            pstTimer = NULL; 
			while (pstNextTimer)
			{
				if (pstNextTimer->nTimeoutCount-- <= 0)
				{
					//* 先从计时器队列摘除
                    if (pstPrevTimer)
                        pstPrevTimer->pstNext = NULL;
                    else
                        l_pstOneShotTimerLink = NULL; 
					pstTimer = pstNextTimer;
                    break; 					
				}
				else //* 计时尚未结束，检查下一个节点
				{
					pstPrevTimer = pstNextTimer;
					pstNextTimer = pstNextTimer->pstNext;
				}
			}
		}
		//os_thread_mutex_unlock(l_hMtxOneShotTimer); 
        os_exit_critical();

        //* 如果存在溢出节点则开始执行溢出操作
        if (pstTimer)
        {
            pstNextTimer = pstTimer;
            while (pstNextTimer)
            {
                //* 保存当前要操作的定时器并在操作之前推进到下一个溢出定时器
                pstTimer = pstNextTimer; 
                pstNextTimer = pstNextTimer->pstNext; 

                //* 执行溢出函数并归还给系统
                pstTimer->pfunTimeoutHandler(pstTimer->pvParam);                           
                one_shot_timer_free(pstTimer);
            }
        }        

		//* 这个休眠可以不用特别精确（1秒左右），我们的应用场景足够了
		os_sleep_secs(1);
	}

	//* 回收资源
	//os_thread_mutex_lock(l_hMtxOneShotTimer);
    os_enter_critical();
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
	//os_thread_mutex_unlock(l_hMtxOneShotTimer);
    os_exit_critical();

    l_ubThreadExitFlag = TRUE;
}

//* 分配一个新的one-shot定时器
PST_ONESHOTTIMER one_shot_timer_new(PFUN_ONESHOTTIMEOUT_HANDLER pfunTimeoutHandler, void *pvParam, INT nTimeoutCount)
{
	PST_ONESHOTTIMER pstTimer = NULL;

    os_critical_init();

	//* 从可用队列中摘取一个空闲节点
	//os_thread_mutex_lock(l_hMtxFreeOneShotTimer);
    os_enter_critical();
	{
		pstTimer = l_pstFreeOneShotTimerLink;
		if (l_pstFreeOneShotTimerLink)
			l_pstFreeOneShotTimerLink = l_pstFreeOneShotTimerLink->pstNext;
	}
	//os_thread_mutex_unlock(l_hMtxFreeOneShotTimer);
    os_exit_critical();

	//* 存在空闲节点则赋值并挂接到计时队列中
	if (pstTimer)
	{
		//* 先赋值再挂载，否则可能导致计数线程出现错误
		pstTimer->pfunTimeoutHandler = pfunTimeoutHandler;
		pstTimer->pvParam = pvParam;
		pstTimer->nTimeoutCount = nTimeoutCount;

		//* 挂接到计时队列中，开始计数
		//os_thread_mutex_lock(l_hMtxOneShotTimer);
        os_enter_critical();
		{
            //* 按照降序挂载到链表
            PST_ONESHOTTIMER pstNextTimer = l_pstOneShotTimerLink; 
            PST_ONESHOTTIMER pstPrevTimer = NULL; 
            while (pstNextTimer)
            {
                if (pstNextTimer->nTimeoutCount > nTimeoutCount)                
                    pstPrevTimer = pstNextTimer; 
                else
                    break;                 

                pstNextTimer = pstNextTimer->pstNext;
            }

            if (pstPrevTimer) //* 说明是中间部分节点
            {
                pstTimer->pstNext = pstPrevTimer->pstNext; 
                pstPrevTimer->pstNext = pstTimer;
            }
            else //* 直接挂载到头部（头部节点计数比新分配的计数要小，直接挂载到头部）
            {
                pstTimer->pstNext = l_pstOneShotTimerLink;
                l_pstOneShotTimerLink = pstTimer;
            }			
		}
		//os_thread_mutex_unlock(l_hMtxOneShotTimer);
        os_exit_critical();
	}

	return pstTimer; 
}

void one_shot_timer_recount(PST_ONESHOTTIMER pstTimer, INT nTimeoutCount)
{
	PST_ONESHOTTIMER pstNextTimer;

	//* 必须大于0才可
	if (nTimeoutCount <= 0)
		return; 

    os_critical_init();

	//*	确保计时队列中还存在这个节点，否则没必要重计数了
	//os_thread_mutex_lock(l_hMtxOneShotTimer);
    os_enter_critical();
	{
        PST_ONESHOTTIMER pstPrevTimer = NULL;
		pstNextTimer = l_pstOneShotTimerLink;        
		while (pstNextTimer)
		{
            //* 找到了这个节点，则先从中摘除以重新排序
			if (pstTimer == pstNextTimer)
			{
                if (pstPrevTimer)                
                    pstPrevTimer->pstNext = pstTimer->pstNext;                
                else                
                    l_pstOneShotTimerLink = l_pstOneShotTimerLink->pstNext;                 
				break; 
			}
             
            pstPrevTimer = pstNextTimer; 
			pstNextTimer = pstNextTimer->pstNext; 
		}

        //* 如果不为空，意味着匹配，则重新排序后挂载到链表上
        if (pstNextTimer)
        {
            pstTimer->nTimeoutCount = nTimeoutCount; 
            
            pstNextTimer = l_pstOneShotTimerLink;
            pstPrevTimer = NULL;
            while (pstNextTimer)
            {
                if (pstNextTimer->nTimeoutCount > nTimeoutCount)
                    pstPrevTimer = pstNextTimer;
                else
                    break;

                pstNextTimer = pstNextTimer->pstNext;
            }

            if (pstPrevTimer) //* 说明是中间部分节点
            {
                pstTimer->pstNext = pstPrevTimer->pstNext;
                pstPrevTimer->pstNext = pstTimer;
            }
            else //* 直接挂载到头部（头部节点计数比新分配的计数要小，直接挂载到头部）
            {
                pstTimer->pstNext = l_pstOneShotTimerLink;
                l_pstOneShotTimerLink = pstTimer;
            }
        }
	}
	//os_thread_mutex_unlock(l_hMtxOneShotTimer); 
    os_exit_critical();
}

//* 这个函数的目的是安全停止计时器并将其归还给系统，不再占用，与one_shot_timer_free()函数不同
//* ，该函数需要先判断其是否依然还在计数，是，则停止并归还给系统，否则不做任何处理
void one_shot_timer_safe_free(PST_ONESHOTTIMER pstTimer)
{
	PST_ONESHOTTIMER pstNextTimer, pstPrevTimer;
	BOOL blIsExist = FALSE; 

    os_critical_init();

	//*	确保计时队列中还存在这个节点，否则不做任何处理
	//os_thread_mutex_lock(l_hMtxOneShotTimer);
    os_enter_critical();
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
	//os_thread_mutex_unlock(l_hMtxOneShotTimer);
    os_exit_critical();

	//* 存在则归还给系统（这里未使用函数调用的方式以减少入栈出栈带来的内存及性能损耗）
	if (blIsExist)
	{
		//os_thread_mutex_lock(l_hMtxFreeOneShotTimer);
        os_enter_critical();
		{
			pstTimer->pstNext = l_pstFreeOneShotTimerLink;
			l_pstFreeOneShotTimerLink = pstTimer;
		}
		//os_thread_mutex_unlock(l_hMtxFreeOneShotTimer);
        os_exit_critical();
	}
}

//* 释放占用的定时器资源，不做任何判断直接释放并归还给系统
void one_shot_timer_free(PST_ONESHOTTIMER pstTimer)
{
    os_critical_init();

	//os_thread_mutex_lock(l_hMtxFreeOneShotTimer);
    os_enter_critical();
	{				
		pstTimer->pstNext = l_pstFreeOneShotTimerLink; 					
		l_pstFreeOneShotTimerLink = pstTimer;
	}
	//os_thread_mutex_unlock(l_hMtxFreeOneShotTimer);
    os_exit_critical(); 
}




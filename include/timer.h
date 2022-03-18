/* timer.h
 *
 * 系统定时器，时间粒度为1秒
 *
 * Neo-T, 创建于2022.03.17 17:45
 * 版本: 1.0
 *
 */
#ifndef TIMER_H
#define TIMER_H

#ifdef SYMBOL_GLOBALS
	#define TIMER_EXT
#else
	#define TIMER_EXT extern
#endif //* SYMBOL_GLOBALS

#define TIMER_NUM	64		//* 定时器数量
typedef struct _ST_ONESHOTTIMER_ { //* 定时器
	struct _ST_ONESHOTTIMER_ *pstNext;
	void(*pfunTimeoutHandler)(struct _ST_ONESHOTTIMER_ *pstTimer, void *pvParam);
	void *pvParam; 
	INT nTimeoutCount;	//* 溢出值，单位：秒	
} ST_ONESHOTTIMER, *PST_ONESHOTTIMER;

TIMER_EXT BOOL timer_init(EN_ERROR_CODE *penErrCode); 
TIMER_EXT void thread_timer_count(void *pvParam);
TIMER_EXT void thread_timeout_handler(void *pvParam);
TIMER_EXT PST_ONESHOTTIMER one_shot_timer_new(INT nTimeoutCount, void(*pfunTimeoutHandler)(void *pvParam), void *pvParam);
TIMER_EXT void one_short_timer_recount(PST_ONESHOTTIMER pstTimer, INT nTimeoutCount);
TIMER_EXT void one_short_timer_stop(PST_ONESHOTTIMER pstTimer); 
TIMER_EXT void one_shot_timer_free(PST_ONESHOTTIMER pstTimer);

#endif
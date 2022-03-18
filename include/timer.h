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
	void(*pfunTimeoutHandler)(void *pvParam);
	void *pvParam; 
	USHORT unStartSecs;		//* 定时器开始时间
	USHORT unTimeoutCount;	//* 溢出值，单位：秒	
} ST_ONESHOTTIMER, *PST_ONESHOTTIMER;

TIMER_EXT BOOL pstack_timer_init(EN_ERROR_CODE *penErrCode); 
TIMER_EXT void pstack_thread_timer_count(void *pvParam);
TIMER_EXT void pstack_thread_timeout_handler(void *pvParam);
TIMER_EXT PST_ONESHOTTIMER pstack_one_shot_timer_new(UINT unTimeoutCount, void(*pfunTimeoutHandler)(void *pvParam), void *pvParam);
TIMER_EXT void pstack_one_shot_timer_free(PST_ONESHOTTIMER pstTimer);

#endif
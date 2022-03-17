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

typedef struct _ST_TIMER_ { //* 定时器
	struct _ST_TIMER_ *pstNext; 
	UINT unStartSecs;		//* 定时器开始时间
	UINT unTimeoutCount;	//* 溢出值，单位：秒
	void(*pfunTimeoutHandler)(void *pvParam); 
} ST_TIMER, *PST_TIMER;

TIMER_EXT BOOL pstack_timer_init(EN_ERROR_CODE *penErrCode); 
TIMER_EXT void pstack_thread_timer_count(void *pvParam);
TIMER_EXT void pstack_thread_timeout_handler(void *pvParam);

#endif
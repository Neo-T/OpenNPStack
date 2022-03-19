/* one_shot_timer.h
 *
 * 协议栈使用的one-shot类型的定时器，时间粒度为1秒
 *
 * Neo-T, 创建于2022.03.17 17:45
 * 版本: 1.0
 *
 */
#ifndef ONE_SHOT_TIMER_H
#define ONE_SHOT_TIMER_H

#ifdef SYMBOL_GLOBALS
	#define ONE_SHOT_TIMER_EXT
#else
	#define ONE_SHOT_TIMER_EXT extern
#endif //* SYMBOL_GLOBALS

#define ONE_SHOT_TIMER_NUM	64		//* 定时器数量

typedef void(*PFUN_ONESHOTTIMEOUT_HANDLER)(void *pvParam); 
typedef struct _ST_ONESHOTTIMER_ { //* 定时器
	struct _ST_ONESHOTTIMER_ *pstNext;
	PFUN_ONESHOTTIMEOUT_HANDLER pfunTimeoutHandler;
	void *pvParam; 
	INT nTimeoutCount;	//* 溢出值，单位：秒	
} ST_ONESHOTTIMER, *PST_ONESHOTTIMER;

//* 定时器初始化函数
ONE_SHOT_TIMER_EXT BOOL one_shot_timer_init(EN_ERROR_CODE *penErrCode);

//* one-shot定时器计数线程
ONE_SHOT_TIMER_EXT void thread_one_shot_timer_count(void *pvParam);

//* 计时移除后该线程负责调用用户自定义的溢出函数并自动销毁定时器资源，用户无需再执行销毁操作
ONE_SHOT_TIMER_EXT void thread_one_shot_timeout_handler(void *pvParam);

//* 创建一个新的定时器，参数pfunTimeoutHandler指定用户自定义的溢出处理函数，pvParam为传递给溢出处理函数的参数，nTimeoutCount指定计数值
ONE_SHOT_TIMER_EXT PST_ONESHOTTIMER one_shot_timer_new(PFUN_ONESHOTTIMEOUT_HANDLER pfunTimeoutHandler, void *pvParam, INT nTimeoutCount);

//* 定时器重新计数以延长超时时间，重新计数的前提是定时器尚未超时溢出，参数pstTimer指定要重新计数的定时器，nTimeoutCount则指定新的计数值
ONE_SHOT_TIMER_EXT void one_short_timer_recount(PST_ONESHOTTIMER pstTimer, INT nTimeoutCount);

//* 停止并安全释放一个正在计时的定时器，如果以超时溢出则不执行任何操作，与one_shot_timer_free()函数不同，该函数需要先判断其是否
//* 依然还在计数，是才会执行释放并归还操作
ONE_SHOT_TIMER_EXT void one_short_timer_safe_free(PST_ONESHOTTIMER pstTimer);

//* 将定时器归还给系统，该函数不做任何判断，直接将参数pstTimer指定的定时器重新连接到系统定时器队列以供用户下次申请使用
ONE_SHOT_TIMER_EXT void one_shot_timer_free(PST_ONESHOTTIMER pstTimer);

#endif
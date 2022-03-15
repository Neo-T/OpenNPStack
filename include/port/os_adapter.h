/* os_adapter.h
*
* 操作系统适配层，在此定义与目标操作系统相关的接口函数
*
* Neo-T, 创建于2022.03.15 13:53
* 版本: 1.0
*
*/
#ifndef OS_ADAPTER_H
#define OS_ADAPTER_H

#ifdef SYMBOL_GLOBALS
#define OS_ADAPTER_EXT
#else
#define OS_ADAPTER_EXT extern
#endif //* SYMBOL_GLOBALS

//* 一些协议栈要用到的与OS适配层相关的全局变量
//* ==============================================================================================
//* ==============================================================================================

//* 一些协议栈要用到的需要OS提供的支撑函数
//* ==============================================================================================
OS_ADAPTER_EXT HMUTEX os_thread_mutex_init(void);			//* 线程同步锁初始化，成功返回同步锁句柄，失败则返回INVALID_HMUTEX
OS_ADAPTER_EXT void os_thread_mutex_lock(HMUTEX hMutex);	//* 线程同步区加锁
OS_ADAPTER_EXT void os_thread_mutex_unlock(HMUTEX hMutex);  //* 线程同步区开锁

#endif


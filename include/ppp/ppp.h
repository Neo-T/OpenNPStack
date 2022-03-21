/* ppp.h
 *
 * 完成ppp模块相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.03.21 10:19
 * 版本: 1.0
 *
 */
#ifndef PPP_H
#define PPP_H

#ifdef SYMBOL_GLOBALS
	#define PPP_EXT
#else
	#define PPP_EXT extern
#endif //* SYMBOL_GLOBALS

PPP_EXT BOOL ppp_init(EN_ERROR_CODE *penErrCode); 

#endif
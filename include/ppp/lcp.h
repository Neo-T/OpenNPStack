/* lcp.h
 *
 * 完成lcp协议相关基础数据结构、宏定义，功能函数声明等相关工作
 *
 * Neo-T, 创建于2022.03.28 18:16
 * 版本: 1.0
 *
 */
#ifndef LCP_H
#define LCP_H

#ifdef SYMBOL_GLOBALS
	#define LCP_EXT
#else
	#define LCP_EXT extern
#endif //* SYMBOL_GLOBALS
#include "negotiation_storage.h"

LCP_EXT BOOL start_negotiation(HTTY hTTY, PST_PPPWAITACKLIST pstWAList, PST_PPPNEGORESULT pstNegoResult, EN_ERROR_CODE *penErrCode);

#endif
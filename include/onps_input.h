/* onps_input.h
 *
 * icmp/tcp/udp层接收处理逻辑用到的基础数据结构、辅助宏、相关功能函数等的定义与声明工作
 *
 * Neo-T, 创建于2022.04.13 17:25
 * 版本: 1.0
 *
 */
#ifndef ONPS_INPUT_H
#define ONPS_INPUT_H

#ifdef SYMBOL_GLOBALS
	#define ONPS_INPUT_EXT
#else
	#define ONPS_INPUT_EXT extern
#endif //* SYMBOL_GLOBALS
#include "ip/ip.h"

typedef struct _ST_ONPS_INPUT_NODE_ {

} ST_ONPS_INPUT_NODE, *PST_ONPS_INPUT_NODE;

#endif

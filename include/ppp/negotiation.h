/* negotiation.h
 *
 * 完成ppp链路的协商工作，为ip层通讯准备好ppp链路
 *
 * Neo-T, 创建于2022.03.24 14:48
 * 版本: 1.0
 *
 */
#ifndef NEGOTIATION_H
#define NEGOTIATION_H

#ifdef SYMBOL_GLOBALS
	#define NEGOTIATION_EXT
#else
	#define NEGOTIATION_EXT extern
#endif //* SYMBOL_GLOBALS
#include "ppp_frame.h"
#include "ppp_utils.h"
#include "tty.h"

 //* ppp链路工作状态
typedef enum {
	TTYINIT = 0,		//* tty终端初始化
	STARTNEGOTIATION,	//* 开启链路协商
	NEGOTIATION,		//* 协商
	ESTABLISHED,		//* 链路已建立
	TERMINATE,			//* 终止链路
	TERMINATED			//* 链路已终止
} EN_PPP_LINK_STATE;

//* PPP控制块
typedef struct _STCB_NETIFPPP_ {
	HTTY hTTY;
	EN_PPP_LINK_STATE enState;
	BOOL blIsThreadExit;
} STCB_NETIFPPP, *PSTCB_NETIFPPP;

//* 与移动运营商协商建立ppp链路，参数pstcbPPP指向ppp链路控制块，用于保存当前ppp链路的协商状态、所使用的tty终端
//* 的句柄等信息，参数pblIsRunning则用于确保上层调用者可以随时终止当前协商过程
NEGOTIATION_EXT void ppp_link_establish(PSTCB_NETIFPPP pstcbPPP, BOOL *pblIsRunning, EN_ERROR_CODE *penErrCode);

#endif
/* chat.h
 *
 * 实现对modem的相关工作状态的检测、迁移等相关操作，比如确定SIM卡是否存在、拨号等操作
 *
 * Neo-T, 创建于2022.03.22 10:17
 * 版本: 1.0
 *
 */
#ifndef CHAT_H
#define CHAT_H

#ifdef SYMBOL_GLOBALS
	#define CHAT_EXT
#else
	#define CHAT_EXT extern
#endif //* SYMBOL_GLOBALS

CHAT_EXT BOOL modem_ready(HTTY hTTY);

#endif
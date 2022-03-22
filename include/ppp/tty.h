/* tty.h
 *
 * tty终端，提供与modem设备进行通讯的相关功能函数
 *
 * Neo-T, 创建于2022.03.22 10:17
 * 版本: 1.0
 *
 */
#ifndef TTY_H
#define TTY_H

#ifdef SYMBOL_GLOBALS
	#define TTY_EXT
#else
	#define TTY_EXT extern
#endif //* SYMBOL_GLOBALS

TTY_EXT HTTY tty_init(const CHAR *pszTTYName, EN_ERROR_CODE *penErrCode); 
TTY_EXT BOOL tty_ready(HTTY hTTY, EN_ERROR_CODE *penErrCode);

#endif
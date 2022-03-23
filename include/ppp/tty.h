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

#define TTY_NUM				1		//* 最多支持几路ppp链路（系统存在几个modem这里就指定几就行）
#define TTY_RCV_BUF_SIZE	2048	//* 指定tty接收缓冲区的大小（一定要大于等于最大ppp帧的长度才可）

typedef struct _STCB_TTY_READ_ {
	HTTY hTTY;
	UCHAR *pubBuf;
	UINT unWriteIdx;	 
} STCB_TTY_READ, *PSTCB_TTY_READ; 

TTY_EXT HTTY tty_init(const CHAR *pszTTYName, EN_ERROR_CODE *penErrCode); 
TTY_EXT void tty_uninit(HTTY hTTY);
TTY_EXT BOOL tty_ready(HTTY hTTY, EN_ERROR_CODE *penErrCode); 
TTY_EXT INT tty_read(HTTY hTTY, UCHAR *pubReadBuf, INT nReadBufLen, EN_ERROR_CODE *penErrCode);

#endif
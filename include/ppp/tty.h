/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.03.22 10:17
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * tty终端，提供与modem设备进行通讯的相关功能函数
 *
 */
#ifndef TTY_H
#define TTY_H

#ifdef SYMBOL_GLOBALS
	#define TTY_EXT
#else
	#define TTY_EXT extern
#endif //* SYMBOL_GLOBALS

#define TTY_RCV_BUF_SIZE	2048	//* 指定tty接收缓冲区的大小（一定要大于等于最大ppp帧的长度才可）
#define TTY_SEND_BUF_SIZE	256		//* 指定tty发送缓冲区的大小，该缓冲区用于发送前ppp转义，其大小可以任意指定，如果内存够用建议比系统要发送的最大数据报稍大些，这样可以一次转义完毕，一次全部发送，而不是分段转义分段发送

typedef struct _STCB_TTYIO_ {
	HTTY hTTY;
	struct {		
		INT nWriteIdx;
        INT nReadIdx; 
        CHAR bState; 
        CHAR bErrCount; 
	} stRecv;

	UCHAR ubaSendBuf[TTY_SEND_BUF_SIZE]; 
} STCB_TTYIO, *PSTCB_TTYIO;

TTY_EXT HTTY tty_init(const CHAR *pszTTYName, EN_ONPSERR *penErr); 
TTY_EXT void tty_uninit(HTTY hTTY);
TTY_EXT BOOL tty_ready(HTTY hTTY, EN_ONPSERR *penErr); 
TTY_EXT INT tty_recv(INT nPPPIdx, HTTY hTTY, UCHAR *pubRecvBuf, INT nRecvBufLen, void(*pfunPacketHandler)(INT, UCHAR *, INT), INT nWaitSecs, EN_ONPSERR *penErr);
TTY_EXT INT tty_send(HTTY hTTY, UINT unACCM, UCHAR *pubData, INT nDataLen, EN_ONPSERR *penErr);
TTY_EXT INT tty_send_ext(HTTY hTTY, UINT unACCM, SHORT sBufListHead, EN_ONPSERR *penErr);

#endif

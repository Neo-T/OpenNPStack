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

#define TTY_RCV_BUF_SIZE	2048	//* 指定tty接收缓冲区的大小（一定要大于等于最大ppp帧的长度才可）
#define TTY_SEND_BUF_SIZE	256		//* 指定tty发送缓冲区的大小，该缓冲区用于发送前ppp转义，其大小可以任意指定，如果内存够用建议比系统要发送的最大数据报稍大些，这样可以一次转义完毕，一次全部发送，而不是分段转义分段发送

typedef struct _STCB_TTYIO_ {
	HTTY hTTY;
	struct {
		UCHAR ubaBuf[TTY_RCV_BUF_SIZE];
		INT nWriteIdx;
	} stRecv;

	UCHAR ubaSendBuf[TTY_SEND_BUF_SIZE];
} STCB_TTYIO, *PSTCB_TTYIO;

TTY_EXT HTTY tty_init(const CHAR *pszTTYName, EN_ERROR_CODE *penErrCode); 
TTY_EXT void tty_uninit(HTTY hTTY);
TTY_EXT BOOL tty_ready(HTTY hTTY, EN_ERROR_CODE *penErrCode); 
TTY_EXT INT tty_recv(HTTY hTTY, UCHAR *pubRecvBuf, INT nRecvBufLen, INT nWaitSecs, EN_ERROR_CODE *penErrCode);
TTY_EXT INT tty_send(HTTY hTTY, UINT unACCM, UCHAR *pubData, INT nDataLen, EN_ERROR_CODE *penErrCode);
TTY_EXT INT tty_send_ext(HTTY hTTY, UINT unACCM, SHORT sBufListHead, EN_ERROR_CODE *penErrCode);

#endif

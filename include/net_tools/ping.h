/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.07.12 14:36
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 提供ping功能实现相关的操作函数
 *
 */
#ifndef PING_H
#define PING_H

#ifdef SYMBOL_GLOBALS
	#define PING_EXT
#else
	#define PING_EXT extern
#endif //* SYMBOL_GLOBALS

#if NETTOOLS_PING

typedef void(*PFUN_PINGRCVHANDLER)(USHORT usIdentifier, void *pvFromAddr, USHORT usSeqNum, UCHAR *pubEchoData, UCHAR ubEchoDataLen, UCHAR ubTTL, UCHAR ubElapsedMSecs, void *pvParam); 
typedef void(*PFUN_PINGERRHANDLER)(USHORT usIdentifier, const CHAR *pszDstAddr, UCHAR ubIcmpErrType, UCHAR ubIcmpErrCode, void *pvParam);

//* 开始ping探测，返回值唯一的标识当前开始的ping探测链路，其用于ping_send()、ping_recv()、ping()、ping_end()函数的调用
PING_EXT INT ping_start(INT family, EN_ONPSERR *penErr);

//* 结束ping探测
PING_EXT void ping_end(INT nPing); 

//* 发送pin探测报文：
//* 参数unDstAddr指定ping目标地址；
//* 参数usSeqNum指定ping报文序号，一般来说这是一个递增序号；
//* 参数ubTTL指定ping报文的ttl值；
//* 参数pubEcho和ubEchoLen指定ping报文携带的echo数据及其长度；
PING_EXT INT ping_send(INT family, INT nPing, const void *pvDstAddr, USHORT usSeqNum, UCHAR ubTTL, const UCHAR *pubEcho, UCHAR ubEchoLen, EN_ONPSERR *penErr);


//* 等待接收ping的应答报文，收到正确的应答报文时返回值为应答报文携带的echo数据的长度，等于0则接收超时，小于0意味着出错，具体原因由参数penErr获取：
//* 参数punFromAddr存储应答报文携带的源地址，如果不需要则参数值可以为NULL；
//* 参数pusSeqNum存储应答报文的序号，序号应与ping_send()函数指定的unSeqNum值一致方可确定收到了正确应答，如果不需要则参数值可以为NULL；
//* 参数pubDataBuf和ubDataBufSize分别指定用于接收应答报文携带的echo数据的缓冲区地址与长度，这两个参数不能为NULL；
//* 参数pubTTL存储应答报文的ttl值，如果不需要则可以为NULL；
//* 参数ubWaitSecs指定接收应答报文的最长等待时间，单位：秒；
PING_EXT INT ping_recv(INT nPing, void *pvFromAddr, USHORT *pusSeqNum, UCHAR *pubDataBuf, UCHAR ubDataBufSize, UCHAR *pubTTL, UCHAR *pubType, UCHAR *pubCode, UCHAR ubWaitSecs, EN_ONPSERR *penErr);

//* 发送并等待接收应答报文，返回值大于0意味着收到正确的应答报文，等于0则接收超时，小于0意味着出错，具体原因通过参数penErr获取：
//* 参数pszDstAddr指定ping目标地址，可读字符串形式；
//* 参数pfunGetCurMSecs指定一个计时函数，其用于记录ping开始时地毫秒数以及结束时的毫秒数，以计算ping目标地址的时长
//* 参数pfunRcvHandler指向用户自定义的接收处理函数，当收到正确的ping应答报文后调用，用户可根据自己需求实现特定处理逻辑
PING_EXT INT ping(INT nPing, const CHAR *pszDstAddr, USHORT usSeqNum, UCHAR ubTTL, UINT(*pfunGetCurMSecs)(void), PFUN_PINGRCVHANDLER pfunRcvHandler, PFUN_PINGERRHANDLER pfunErrHandler, UCHAR ubWaitSecs, void *pvParam, EN_ONPSERR *penErr);

#if NETTOOLS_TELNETSRV && NVTCMD_PING_EN
typedef struct _ST_PING_STARTARGS_ { //* ping启动参数
    CHAR bIsCpyEnd;
    ULONGLONG ullNvtHandle;
    INT nFamily; 
    CHAR szDstIp[40]; 
} ST_PING_STARTARGS, *PST_PING_STARTARGS; 
PING_EXT void nvt_cmd_ping_entry(void *pvParam); 
#endif //* #if NETTOOLS_TELNETSRV && NVTCMD_PING_EN
#endif
#endif

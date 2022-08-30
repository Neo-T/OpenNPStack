/* ping.h
 *
 * 提供ping功能实现相关的操作函数
 * 
 * Neo-T, 创建于2022.07.12 14:36
 * 版本: 1.0
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

//* 开始ping探测，返回值唯一的标识当前开始的ping探测链路，其用于ping_send()、ping_recv()、ping()、ping_end()函数的调用
PING_EXT INT ping_start(EN_ONPSERR *penErr);

//* 结束ping探测
PING_EXT void ping_end(INT nPing); 

//* 发送pin探测报文：
//* 参数unDstAddr指定ping目标地址；
//* 参数usSeqNum指定ping报文序号，一般来说这是一个递增序号；
//* 参数ubTTL指定ping报文的ttl值；
//* 参数pubEcho和ubEchoLen指定ping报文携带的echo数据及其长度；
PING_EXT INT ping_send(INT nPing, in_addr_t unDstAddr, USHORT usSeqNum, UCHAR ubTTL, const UCHAR *pubEcho, UCHAR ubEchoLen, EN_ONPSERR *penErr);

//* 等待接收ping的应答报文，收到正确的应答报文时返回值为应答报文携带的echo数据的长度，等于0则接收超时，小于0意味着出错，具体原因由参数penErr获取：
//* 参数punFromAddr存储应答报文携带的源地址，如果不需要则参数值可以为NULL；
//* 参数pusSeqNum存储应答报文的序号，序号应与ping_send()函数指定的unSeqNum值一致方可确定收到了正确应答，如果不需要则参数值可以为NULL；
//* 参数pubDataBuf和ubDataBufSize分别指定用于接收应答报文携带的echo数据的缓冲区地址与长度，这两个参数不能为NULL；
//* 参数pubTTL存储应答报文的ttl值，如果不需要则可以为NULL；
//* 参数ubWaitSecs指定接收应答报文的最长等待时间，单位：秒；
PING_EXT INT ping_recv(INT nPing, in_addr_t *punFromAddr, USHORT *pusSeqNum, UCHAR *pubDataBuf, UCHAR ubDataBufSize, UCHAR *pubTTL, UCHAR ubWaitSecs, EN_ONPSERR *penErr); 

//* 发送并等待接收应答报文，返回值大于0意味着收到正确的应答报文，等于0则接收超时，小于0意味着出错，具体原因通过参数penErr获取：
//* 参数unDstAddr指定ping目标地址；
//* 参数pfunGetCurMSecs指定一个计时函数，其用于记录ping开始时地毫秒数以及结束时的毫秒数，以计算ping目标地址的时长
//* 参数pfunRcvHandler指向用户自定义的接收处理函数，当收到正确的ping应答报文后调用，用户可根据自己需求实现特定处理逻辑
PING_EXT INT ping(INT nPing, in_addr_t unDstAddr, USHORT usSeqNum, UCHAR ubTTL, UINT(*pfunGetCurMSecs)(void), void(* pfunRcvHandler)(USHORT usIdentifier, in_addr_t unFromAddr, USHORT usSeqNum, UCHAR *pubEchoData, UCHAR ubEchoDataLen, UCHAR ubTTL, UCHAR ubElapsedMSecs), UCHAR ubWaitSecs, EN_ONPSERR *penErr);
#endif
#endif

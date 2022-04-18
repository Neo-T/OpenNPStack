/* wait_ack_list.h
 *
 * 提供针对ppp协商阶段以及链路保持阶段需要等待应答的报文链表队列的相关管理函数声明及辅助宏定义
 *
 * Neo-T, 创建于2022.03.29 10:34
 * 版本: 1.0
 *
 */
#ifndef WAIT_ACK_LIST_H
#define WAIT_ACK_LIST_H

#ifdef SYMBOL_GLOBALS
	#define WAIT_ACK_LIST_EXT
#else
	#define WAIT_ACK_LIST_EXT extern
#endif //* SYMBOL_GLOBALS
#include "one_shot_timer.h"

typedef struct _ST_PPPWAITACKNODE_ { //* 等待应答的PPP报文链表节点
	struct _ST_PPPWAITACKNODE_ *pstPrev;
	struct _ST_PPPWAITACKNODE_ *pstNext;
	UCHAR ubIsAcked; 
	struct {
		UCHAR ubCode;
		UCHAR ubIdentifier;				
		USHORT usProtocol;
	} stPacket;	
	PST_ONESHOTTIMER pstTimer;
	struct _ST_PPPWAITACKLIST_ *pstList; 
} ST_PPPWAITACKNODE, *PST_PPPWAITACKNODE;

typedef struct _ST_PPPWAITACKLIST_ { //* 等待应答的PPP报文链表
	PST_PPPWAITACKNODE pstHead;
	HMUTEX hMutex;
	UCHAR ubIsTimeout; 
	UCHAR ubTimeoutCount;	//* 连续触发超时事件的累计次数
} ST_PPPWAITACKLIST, *PST_PPPWAITACKLIST;

WAIT_ACK_LIST_EXT BOOL wait_ack_list_init(PST_PPPWAITACKLIST pstWAList, EN_ONPSERR *penErr);
WAIT_ACK_LIST_EXT void wait_ack_list_uninit(PST_PPPWAITACKLIST pstWAList);
WAIT_ACK_LIST_EXT BOOL wait_ack_list_add(PST_PPPWAITACKLIST pstWAList, USHORT usProtocol, UCHAR ubCode, UCHAR ubIdentifier, INT nTimerCount, EN_ONPSERR *penErr);
WAIT_ACK_LIST_EXT void wait_ack_list_del(PST_PPPWAITACKLIST pstWAList, USHORT usProtocol, UCHAR ubIdentifier);

#endif

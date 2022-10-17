/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"

#if SUPPORT_PPP
#define SYMBOL_GLOBALS
#include "ppp/wait_ack_list.h"
#undef SYMBOL_GLOBALS

#define PPP_WAIT_ACK_NODE_NUM  5 //* 分配的应答等待节点的数量，每个ppp链路拨号时使用4个，多出几个留点裕富
static ST_PPPWAITACKNODE l_staaPPPWaitAckNode[PPP_NETLINK_NUM][PPP_WAIT_ACK_NODE_NUM];

//* 等待应答队列
BOOL wait_ack_list_init(PST_PPPWAITACKLIST pstWAList, EN_ONPSERR *penErr)
{	
    if (pstWAList->bPPPIdx < 0 || pstWAList->bPPPIdx >= PPP_NETLINK_NUM)
    {
        if (penErr)
            *penErr = ERRPPPIDXOVERFLOW;
        return FALSE;
    }

	pstWAList->hMutex = os_thread_mutex_init();
    if (INVALID_HMUTEX == pstWAList->hMutex)
    {
        if(penErr)
            *penErr = ERRMUTEXINITFAILED; 
        return FALSE;
    }

    INT i;
    for (i = 0; i < PPP_WAIT_ACK_NODE_NUM; i++)
        l_staaPPPWaitAckNode[pstWAList->bPPPIdx][i].bIsUsed = FALSE;
	
	pstWAList->pstHead = NULL;
	pstWAList->ubTimeoutCount = 0;

	return TRUE;
}

void wait_ack_list_uninit(PST_PPPWAITACKLIST pstWAList)
{
	if (pstWAList && INVALID_HMUTEX != pstWAList->hMutex)
	{
		os_thread_mutex_lock(pstWAList->hMutex);
		{
			//* 销毁所有尚未等到应答的节点资源，避免内存泄露
			PST_PPPWAITACKNODE pstNextNode = pstWAList->pstHead;
			while (pstNextNode)
			{
				one_shot_timer_recount(pstNextNode->pstTimer, 1); //* 其实还是交给超时函数去统一处理，这样才可确保对该链表的访问不会冲突
				pstNextNode = pstNextNode->pstNext;  //* 移动到下一个节点				
			}
		}
		os_thread_mutex_unlock(pstWAList->hMutex);

		//* 等待所有节点释放完毕
        while (pstWAList->pstHead)
            os_sleep_ms(10);

		//* 销毁互斥锁
		os_thread_mutex_uninit(pstWAList->hMutex);
		pstWAList->hMutex = INVALID_HMUTEX; 
	}
}

static PST_PPPWAITACKNODE wait_ack_list_get_free_node(PST_PPPWAITACKLIST pstWAList)
{
    PST_PPPWAITACKNODE pstNode = NULL; 
    os_thread_mutex_lock(pstWAList->hMutex);
    {
        INT i;
        for (i = 0; i < PPP_WAIT_ACK_NODE_NUM; i++)
        {
            if (!l_staaPPPWaitAckNode[pstWAList->bPPPIdx][i].bIsUsed)
            {
                pstNode = &l_staaPPPWaitAckNode[pstWAList->bPPPIdx][i]; 
                pstNode->bIsUsed = TRUE; 
                break; 
            }
        }
    }
    os_thread_mutex_unlock(pstWAList->hMutex);

    return pstNode;
}

static void wait_ack_list_free_node(PST_PPPWAITACKLIST pstWAList, PST_PPPWAITACKNODE pstFreedNode)
{
	if (!pstWAList || INVALID_HMUTEX == pstWAList->hMutex || !pstFreedNode)
		return; 

	//* 从链表中摘除
	if (pstFreedNode->pstPrev)
		pstFreedNode->pstPrev->pstNext = pstFreedNode->pstNext;
	if (pstFreedNode->pstNext)
		pstFreedNode->pstNext->pstPrev = pstFreedNode->pstPrev;
	if (pstWAList->pstHead == pstFreedNode)
		pstWAList->pstHead = pstFreedNode->pstNext;

    //* 归还当前已经结束使命的wait ack节点
    pstFreedNode->bIsUsed = FALSE; 
}

static void wait_ack_timeout_handler(void *pvParam)
{
	PST_PPPWAITACKNODE pstTimeoutNode = (PST_PPPWAITACKNODE)pvParam; 
	PST_PPPWAITACKLIST pstWAList = pstTimeoutNode->pstList; 
	
	os_thread_mutex_lock(pstWAList->hMutex);
	{
		if (pstTimeoutNode->bIsAcked) //* 如果已经收到应答报文，则错误计数归零
			pstWAList->ubTimeoutCount = 0;
		else //* 超时了，当前节点记录的报文未收到应答
		{
			if (pstTimeoutNode->bIsAcked) //* 存在这种情况，等待进入临界段时好巧不巧收到应答报文了			
				pstWAList->ubTimeoutCount = 0;
			else
			{
				pstWAList->ubIsTimeout = TRUE; 
				pstWAList->ubTimeoutCount++;
			}
		}

		//* 直接释放当前节点即可，不需要单独释放申请的定时器，定时器超时后会自动归还给系统
		wait_ack_list_free_node(pstWAList, pstTimeoutNode);         
	}
	os_thread_mutex_unlock(pstWAList->hMutex);	
}

BOOL wait_ack_list_add(PST_PPPWAITACKLIST pstWAList, USHORT usProtocol, UCHAR ubCode, UCHAR ubIdentifier, INT nTimerCount, EN_ONPSERR *penErr)
{
	if (!pstWAList || INVALID_HMUTEX == pstWAList->hMutex)
	{
		if (penErr)
			*penErr = ERRPPPWALISTNOINIT;
		return FALSE;
	}

	//* 申请一块内存以建立一个新的队列节点
    PST_PPPWAITACKNODE pstNewNode = wait_ack_list_get_free_node(pstWAList); 
    if (!pstNewNode)
    {
        if (penErr)
            *penErr = ERRNOFREEPPWANODE;
        
        return FALSE;
    }

	//* 新建一个定时器用于等待应答计时
	pstNewNode->pstTimer = one_shot_timer_new(wait_ack_timeout_handler, pstNewNode, nTimerCount);
	if (!pstNewNode->pstTimer)
	{
		//buddy_free(pstNewNode);

		if (penErr)
			*penErr = ERRNOIDLETIMER;
		return FALSE;
	}
	//* 记录等待应答报文的关键特征数据
	pstNewNode->stPacket.usProtocol = usProtocol; 
	pstNewNode->stPacket.ubCode = ubCode; 
	pstNewNode->stPacket.ubIdentifier = ubIdentifier; 
	//* 尚未等到应答
	pstNewNode->bIsAcked = FALSE; 

	//* 记录链表首地址，超时处理函数需要
	pstNewNode->pstList = pstWAList;

	//* 加入队列
	pstNewNode->pstNext = NULL;
	pstNewNode->pstPrev = NULL; 
	os_thread_mutex_lock(pstWAList->hMutex);
	{
		pstNewNode->pstNext = pstWAList->pstHead;
		if (pstWAList->pstHead)
			pstWAList->pstHead->pstPrev = pstNewNode;
		pstWAList->pstHead = pstNewNode;
	}
	os_thread_mutex_unlock(pstWAList->hMutex);

	return TRUE;
}

void wait_ack_list_del(PST_PPPWAITACKLIST pstWAList, USHORT usProtocol, UCHAR ubIdentifier)
{
	if (!pstWAList || INVALID_HMUTEX == pstWAList->hMutex)
		return; 

	os_thread_mutex_lock(pstWAList->hMutex);
	{
		PST_PPPWAITACKNODE pstNextNode = pstWAList->pstHead;
		while (pstNextNode)
		{
			if (usProtocol == pstNextNode->stPacket.usProtocol && ubIdentifier == pstNextNode->stPacket.ubIdentifier)
			{
				pstNextNode->bIsAcked = TRUE;
				one_shot_timer_recount(pstNextNode->pstTimer, 1); //* 其实还是交给超时函数去统一处理，这样才可确保对该链表的访问不会冲突，因为存在应答收到这一刻同时定时器超时溢出的情况，所以
																  //* 在这里我们只是打上已收到应答的标志，同时调整定时器1秒后溢出，由定时器超时函数统一回收相关资源
				break; 
			}

			pstNextNode = pstNextNode->pstNext;
		}
	}
	os_thread_mutex_unlock(pstWAList->hMutex);
}

#endif

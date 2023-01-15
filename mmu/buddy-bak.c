/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "port/sys_config.h"
#include "onps_errors.h"

#define SYMBOL_GLOBALS
#include "mmu/buddy.h"
#undef SYMBOL_GLOBALS

//* 在此把相关基础数据准备好，确保协议栈不过多占用用户的堆空间
static UCHAR l_ubaMemPool[BUDDY_MEM_SIZE];
static ST_BUDDY_AREA l_staArea[BUDDY_ARER_COUNT];
static ST_BUDDY_PAGE l_staPage[BUDDY_MEM_SIZE / BUDDY_PAGE_SIZE + 1];
#if 0
static STCB_BUDDY_PAGE_NODE l_stcbaFreePage[BUDDY_MEM_SIZE / BUDDY_PAGE_SIZE + 1];
#else
static PST_BUDDY_PAGE l_pstFreePageHead; 
#endif
//static HMUTEX l_hMtxMMUBuddy = INVALID_HMUTEX;

static PST_BUDDY_PAGE GetPageNode(EN_ONPSERR *penErr)
{
	PST_BUDDY_PAGE pstPage;

#if 0
	PSTCB_BUDDY_PAGE_NODE pstNode = &l_stcbaFreePage[0];
	while (pstNode)
	{
		if (pstNode->pstPage)
		{
			pstNode->pstPage->blIsUsed = FALSE;
			pstNode->pstPage->pstNext = NULL;
			pstPage = pstNode->pstPage;
			pstNode->pstPage = NULL;
			return pstPage;
		}

		pstNode = pstNode->pstNext; 
	}
#else
    if (l_pstFreePageHead)
    {
        pstPage = l_pstFreePageHead; 
        l_pstFreePageHead = l_pstFreePageHead->pstNext; 
        pstPage->blIsUsed = FALSE; 
        pstPage->pstNext = NULL; 
        return pstPage; 
    }
#endif

    if(penErr)
	    *penErr = ERRNOPAGENODE;

	return NULL;
}

static void FreePageNode(PST_BUDDY_PAGE pstPage)
{
#if 0
	PSTCB_BUDDY_PAGE_NODE pstNode = &l_stcbaFreePage[0];
	while (pstNode)
	{
		if (!pstNode->pstPage)
		{
			pstNode->pstPage = pstPage;
			return;
		}

		pstNode = pstNode->pstNext;
	}
#else   
    pstPage->pstNext = l_pstFreePageHead;
    l_pstFreePageHead = pstPage;    
#endif
}

BOOL buddy_init(EN_ONPSERR *penErr)
{
	INT i;
	UINT unPageSize = BUDDY_PAGE_SIZE; 

#if 0
	//* 存储页面控制信息的链表必须先初始化，接下来就要用到
	for (i = 0; i < (INT)(BUDDY_MEM_SIZE / BUDDY_PAGE_SIZE + 1); i++)
	{
		l_stcbaFreePage[i].pstPage = &l_staPage[i];
		l_stcbaFreePage[i].pstNext = &l_stcbaFreePage[i + 1];
	}
	l_stcbaFreePage[i - 1].pstNext = NULL;
#else
    memset(&l_staPage, 0, sizeof(l_staPage)); 
    for (i = 0; i < (INT)(BUDDY_MEM_SIZE / BUDDY_PAGE_SIZE + 1); i++)    
        l_staPage[i].pstNext = &l_staPage[i + 1];         
    l_staPage[i - 1].pstNext = NULL; 
    l_pstFreePageHead = &l_staPage[0]; 
#endif

	//* 清零，并把交给buddy管理的整块内存挂载到页块管理数组的最后一个单元，确保最后一个单元为最大一块连续的内存
	memset(&l_staArea, 0, sizeof(l_staArea));
	PST_BUDDY_PAGE pstPage = GetPageNode(penErr);
	pstPage->pstNext = NULL;
	pstPage->pubStart = l_ubaMemPool;
	l_staArea[BUDDY_ARER_COUNT - 1].pstNext = pstPage;

	//* 计算并存储各页块管理单元的单个页面大小
	l_staArea[0].unPageSize = unPageSize;
	for (i = 1; i < BUDDY_ARER_COUNT; i++)
	{
		unPageSize *= 2;
		l_staArea[i].unPageSize = unPageSize;
	}

#if 0
	l_hMtxMMUBuddy = os_thread_mutex_init();
	if (INVALID_HMUTEX != l_hMtxMMUBuddy)	
		return TRUE; 
	
	*penErr = ERRMUTEXINITFAILED; 
	return FALSE; 
#else
    return TRUE; 
#endif
}

void buddy_uninit(void)
{
    //if (INVALID_HMUTEX != l_hMtxMMUBuddy)
    //    os_thread_mutex_uninit(l_hMtxMMUBuddy);
}

void *buddy_alloc(UINT unSize, EN_ONPSERR *penErr)
{
	INT i;
	UINT unPageSize = BUDDY_PAGE_SIZE;
	PST_BUDDY_PAGE pstPage, pstPrevPage, pstNextPage;
	PST_BUDDY_AREA pstArea;
	PST_BUDDY_PAGE pstFreePage1, pstFreePage2;

	if (unSize > BUDDY_MEM_SIZE)
	{
		*penErr = ERRREQMEMTOOLARGE;
		return (void *)0;
	}

	//* 确定页块索引
	for (i = 1; i < BUDDY_ARER_COUNT; i++)
	{
		if (unPageSize >= unSize)
		{
			i--;
			break;
		}

		unPageSize *= 2;
	}
	
    os_critical_init(); 

	//* 查找可用且大小适合的页面分配给用户
	//os_thread_mutex_lock(l_hMtxMMUBuddy);
    os_enter_critical();
	{
		//* 存在有效页面则直接返回
		pstPage = l_staArea[i].pstNext;
		while (pstPage)
		{
			if (!pstPage->blIsUsed)
			{
				pstPage->blIsUsed = TRUE;
				//os_thread_mutex_unlock(l_hMtxMMUBuddy);                

                printf("+%p, %u\r\n", pstPage->pubStart, unSize);

                os_exit_critical();
                
				return pstPage->pubStart;
			}

			pstPage = pstPage->pstNext;
		}

		//* 一旦执行到这里意味着当前页块单元没有可用页块，需要从更大的页块单元分裂出一块来使用
		//* 先找到最近的有空余页块的单元	
		i++;
		for (; i < BUDDY_ARER_COUNT; i++)
		{
			pstPage = l_staArea[i].pstNext;
			pstPrevPage = NULL;
			while (pstPage)
			{
				if (!pstPage->blIsUsed)
					goto __lblSplit;
				pstPrevPage = pstPage;
				pstPage = pstPage->pstNext;
			}
		}
		//os_thread_mutex_unlock(l_hMtxMMUBuddy);
        os_exit_critical(); 

		*penErr = ERRNOFREEMEM; //* 没有空余页块，无法分配内存给用户了

		return (void *)0;

	__lblSplit:
		for (; i > 0; i--)
		{
			//* 摘除
			if (pstPrevPage)
				pstPrevPage->pstNext = pstPage->pstNext;
			else
				l_staArea[i].pstNext = pstPage->pstNext;

			//* 分裂
			pstArea = &l_staArea[i - 1];
			pstFreePage1 = GetPageNode(penErr);
			if (!pstFreePage1) //* 这属于程序BUG，理论上不应该申请不到
			{
				//os_thread_mutex_unlock(l_hMtxMMUBuddy);
                os_exit_critical();
				return (void *)0;
			}
			pstFreePage2 = GetPageNode(penErr);
			if (!pstFreePage2) //* 同上
			{
				//os_thread_mutex_unlock(l_hMtxMMUBuddy);
                os_exit_critical();
				return (void *)0;
			}
			pstFreePage1->pubStart = pstPage->pubStart;
			pstFreePage2->pubStart = pstPage->pubStart + pstArea->unPageSize;
			FreePageNode(pstPage); 

			//* 挂接到下一个更小的页块单元
			pstNextPage = pstArea->pstNext;
			if (!pstNextPage)
			{
				pstArea->pstNext = pstFreePage1;
				pstFreePage1->pstNext = pstFreePage2;
				pstPage = pstFreePage1;
				pstPrevPage = NULL;
			}
			else
			{
				do {
					if (!pstNextPage->pstNext)
					{
						pstNextPage->pstNext = pstFreePage1;
						pstFreePage1->pstNext = pstFreePage2;
						pstPage = pstFreePage1;
						pstPrevPage = pstNextPage;
						break;
					}

					pstNextPage = pstNextPage->pstNext;
				} while (TRUE);
			}

			//* 看看当前单元满足分配要求吗，满足则分配，否则继续分裂		
			if (unSize > pstArea->unPageSize / 2 || BUDDY_PAGE_SIZE == pstArea->unPageSize)
			{
				//* 申请容量大于页块容量的一半，不需要继续分裂了，直接返回给用户即可
				pstPage->blIsUsed = TRUE;
				//os_thread_mutex_unlock(l_hMtxMMUBuddy); 

                printf("+%p, %u\r\n", pstPage->pubStart, unSize);

                os_exit_critical();
                
				return (void *)pstPage->pubStart;
			}
		}
	}
	//os_thread_mutex_unlock(l_hMtxMMUBuddy);
    os_exit_critical();

	*penErr = ERRNOFREEMEM; //* 理论上这里是执行不到的

	return (void *)0;
}

//* 计算伙伴地址
static UCHAR *cal_buddy_addr(PST_BUDDY_AREA pstArea, PST_BUDDY_PAGE pstPage)
{
	UINT unOffset = (UINT)(pstPage->pubStart - l_ubaMemPool); 
	UINT unUpperPageSize = pstArea->unPageSize * 2;
	if (unOffset % unUpperPageSize == 0) //* 伙伴地址为后半部分	
		return pstPage->pubStart + pstArea->unPageSize; 	
	else //* 前半部分
		return (unOffset > unUpperPageSize) ? &l_ubaMemPool[((unOffset / unUpperPageSize) * unUpperPageSize)] : l_ubaMemPool;
}

BOOL buddy_free(void *pvStart)
{
	INT i;
	PST_BUDDY_AREA pstArea;
	PST_BUDDY_PAGE pstNextPage, pstPrevPage1, pstPrevPage2, pstFreedPage;
	UCHAR *pubBuddyAddr;    

    os_critical_init();

	//os_thread_mutex_lock(l_hMtxMMUBuddy);
    os_enter_critical();
	{
		for (i = 0; i < BUDDY_ARER_COUNT; i++)
		{
			pstNextPage = l_staArea[i].pstNext;
			pstPrevPage1 = NULL;
			while (pstNextPage)
			{
				if (pstNextPage->pubStart == (UCHAR *)pvStart)
				{
					pstFreedPage = pstNextPage;
					pstNextPage->blIsUsed = FALSE;
					goto __lblMergeRef;
				}

				pstPrevPage1 = pstNextPage;
				pstNextPage = pstNextPage->pstNext;
			}
		}
		//os_thread_mutex_unlock(l_hMtxMMUBuddy);
        os_exit_critical();

		//* 如果上层调用者擅自修改了分配的起始地址，那么释放就会失败,所以只要释放失败就是这个问题        
		return FALSE; 

    __lblMergeRef: 
        printf("-%p\r\n", pvStart);


		//* 合并操作
    __lblMerge:        
		pstArea = &l_staArea[i];
		pubBuddyAddr = cal_buddy_addr(pstArea, pstFreedPage);

		pstNextPage = pstArea->pstNext;
		pstPrevPage2 = NULL;
		while (pstNextPage)
		{
			if (!pstNextPage->blIsUsed)
			{
				if (pubBuddyAddr == pstNextPage->pubStart) //* 合并
				{
					if (pstFreedPage->pubStart < pstNextPage->pubStart) //* 当前被释放的节点在前
					{
						//* 摘除其实就等于合并了
						if (pstPrevPage1)
							pstPrevPage1->pstNext = pstNextPage->pstNext;
						else
							pstArea->pstNext = pstNextPage->pstNext;
					}
					else
					{
						if (pstPrevPage2)
							pstPrevPage2->pstNext = pstFreedPage->pstNext;
						else
							pstArea->pstNext = pstFreedPage->pstNext;
						pstFreedPage->pubStart = pstNextPage->pubStart;
					}

					FreePageNode(pstNextPage);

					goto __lblMountToUpperLink;
				}
			}

			pstPrevPage2 = pstNextPage;
			pstNextPage = pstNextPage->pstNext;
		}        
		//os_thread_mutex_unlock(l_hMtxMMUBuddy);
        os_exit_critical();

		return TRUE; //* 没有合并节点则结束执行

	__lblMountToUpperLink: //* 将合并后的节点挂载到上层
		i++;
		if (i < BUDDY_ARER_COUNT)
		{
			pstArea = &l_staArea[i];
			pstNextPage = pstArea->pstNext;
			if (!pstNextPage)
			{
				pstArea->pstNext = pstFreedPage;
				pstFreedPage->pstNext = NULL;
                
				//os_thread_mutex_unlock(l_hMtxMMUBuddy);
                os_exit_critical();

				return TRUE;
			}

			pubBuddyAddr = cal_buddy_addr(pstArea, pstFreedPage);
			pstPrevPage2 = NULL;
			while (pstNextPage)
			{
				if (pubBuddyAddr == pstNextPage->pubStart) //* 伙伴页面，则挂载
				{
					if (pstFreedPage->pubStart < pstNextPage->pubStart) //* 要挂载的节点为伙伴的前半部分
					{
						if (pstPrevPage2)
							pstPrevPage2->pstNext = pstFreedPage;
						else
							pstArea->pstNext = pstFreedPage;
						pstFreedPage->pstNext = pstNextPage;

						pstPrevPage1 = pstPrevPage2;
					}
					else
					{
						pstFreedPage->pstNext = pstNextPage->pstNext;
						pstNextPage->pstNext = pstFreedPage;
						pstPrevPage1 = pstNextPage;
					}

					goto __lblMerge;
				}

				pstPrevPage2 = pstNextPage;
				pstNextPage = pstNextPage->pstNext;
			}
		}
	}    
	//os_thread_mutex_unlock(l_hMtxMMUBuddy);
    os_exit_critical();

	return TRUE; 
}

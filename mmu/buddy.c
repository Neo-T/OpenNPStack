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
        //pstPage->blIsUsed = FALSE; 
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
	l_staArea[BUDDY_ARER_COUNT - 1].pstFreed = pstPage;

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
	INT i, nAreaIdx;
	UINT unPageSize = BUDDY_PAGE_SIZE;
	PST_BUDDY_PAGE pstPage, pstNextPage;
	PST_BUDDY_AREA pstArea;
	PST_BUDDY_PAGE pstFreePage1, pstFreePage2;

	if (unSize > BUDDY_MEM_SIZE)
	{
		*penErr = ERRREQMEMTOOLARGE;
		return (void *)0;
	}

	//* 确定页块索引
	for (i = 0; i < BUDDY_ARER_COUNT; i++)
	{
        if (unPageSize >= unSize)
        {
            nAreaIdx = i; 
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
        if (l_staArea[i].pstFreed)
        {
            pstPage = l_staArea[i].pstFreed;
            l_staArea[i].pstFreed = pstPage->pstNext; 
            pstPage->pstNext = l_staArea[i].pstUsed; 
            l_staArea[i].pstUsed = pstPage;
            
            os_exit_critical();

            return pstPage->pubStart;
        }

		//* 一旦执行到这里意味着当前页块单元没有可用页块，需要从更大的页块单元分裂出一块来使用
		//* 先找到最近的有空余页块的单元	
		i++;
		for (; i < BUDDY_ARER_COUNT; i++)
		{
            if (l_staArea[i].pstFreed)
            {
                pstPage = l_staArea[i].pstFreed;
                l_staArea[i].pstFreed = pstPage->pstNext;
                goto __lblSplit;
            }
		}
		//os_thread_mutex_unlock(l_hMtxMMUBuddy);
        os_exit_critical(); 

		*penErr = ERRNOFREEMEM; //* 没有空余页块，无法分配内存给用户了

		return (void *)0;

	__lblSplit:
		for (; i > 0; i--)
		{			
			//* 准备分裂
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

            //* 分裂
			pstFreePage1->pubStart = pstPage->pubStart;
			pstFreePage2->pubStart = pstPage->pubStart + pstArea->unPageSize;
			FreePageNode(pstPage); 

			//* 挂载到页块尾部
            pstPage = pstFreePage1;
			pstNextPage = pstArea->pstFreed;
			if (!pstNextPage)			
				pstArea->pstFreed = pstFreePage2;			
			else
			{
				do {
					if (!pstNextPage->pstNext)
					{
						pstNextPage->pstNext = pstFreePage2;						
						break;
					}

					pstNextPage = pstNextPage->pstNext;
				} while (TRUE);
			}
            
            //* 是否分裂完成
            if (nAreaIdx == i - 1) 
            {
                pstPage->pstNext = pstArea->pstUsed;
                pstArea->pstUsed = pstPage;               

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
		return pstPage->pubStart - pstArea->unPageSize;
}

static void buddy_insert_freed_page(PST_BUDDY_AREA pstArea, PST_BUDDY_PAGE pstFreedPage)
{
    PST_BUDDY_PAGE pstNextPage, pstPrevPage; 

    //* 插入当前链表，注意插入的时候要按照地址顺序插入，不能乱序
    if (pstArea->pstFreed)
    {
        pstNextPage = pstArea->pstFreed;
        pstPrevPage = NULL;
        while (pstNextPage)
        {
            if (pstNextPage->pubStart > pstFreedPage->pubStart)
            {
                if (pstPrevPage)
                {
                    pstPrevPage->pstNext = pstFreedPage;
                    pstFreedPage->pstNext = pstNextPage;
                }
                else
                {
                    pstFreedPage->pstNext = pstArea->pstFreed;
                    pstArea->pstFreed = pstFreedPage;

                }
                return; 
            }

            pstPrevPage = pstNextPage; 
            pstNextPage = pstNextPage->pstNext;
        }

        //* 放到尾部
        pstPrevPage->pstNext = pstFreedPage;
    }
    else    
        pstArea->pstFreed = pstFreedPage;            
    pstFreedPage->pstNext = NULL;
}

BOOL buddy_free(void *pvStart)
{
	INT i;
	PST_BUDDY_AREA pstArea;
	PST_BUDDY_PAGE pstNextPage, pstPrevPage, pstFreedPage;
	UCHAR *pubBuddyAddr;    

    os_critical_init();

	//os_thread_mutex_lock(l_hMtxMMUBuddy);
    os_enter_critical();
	{
		for (i = 0; i < BUDDY_ARER_COUNT; i++)
		{
            if (l_staArea[i].pstUsed)
            {
                pstNextPage = l_staArea[i].pstUsed;
                pstPrevPage = NULL;
                while (pstNextPage)
                {
                    if (pstNextPage->pubStart == (UCHAR *)pvStart)
                    {                        
                        pstFreedPage = pstNextPage;

                        //* 摘除之
                        if (pstPrevPage)                        
                            pstPrevPage->pstNext = pstNextPage->pstNext;                         
                        else
                            l_staArea[i].pstUsed = pstNextPage->pstNext; 

                        pstArea = &l_staArea[i];
                        //pstNextPage->blIsUsed = FALSE;
                        goto __lblMerge;
                    }

                    pstPrevPage = pstNextPage;
                    pstNextPage = pstNextPage->pstNext;
                }
            }			
		}
		//os_thread_mutex_unlock(l_hMtxMMUBuddy);
        os_exit_critical();

		//* 如果上层调用者擅自修改了分配的起始地址，那么释放就会失败,所以只要释放失败就是这个问题        
		return FALSE; 

		//* 合并操作
    __lblMerge:        		
		pubBuddyAddr = cal_buddy_addr(pstArea, pstFreedPage);

		pstNextPage = pstArea->pstFreed;
		pstPrevPage = NULL;
		while (pstNextPage)
		{
            if (pubBuddyAddr == pstNextPage->pubStart) //* 其伙伴节点未被使用，则直接摘除
            {
                //* 先摘除之
                if (pstPrevPage)
                    pstPrevPage->pstNext = pstNextPage->pstNext;
                else
                    pstArea->pstFreed = pstNextPage->pstNext; 

                //* 当前被释放的节点在后，则将地址调整到伙伴页面的前半部分，这就相当于合并了
                if (pstFreedPage->pubStart > pstNextPage->pubStart)
                    pstFreedPage->pubStart = pstNextPage->pubStart;

                //* 释放伙伴节点
                FreePageNode(pstNextPage);
                goto __lblMountToUpperLink;
            }			

			pstPrevPage = pstNextPage;
			pstNextPage = pstNextPage->pstNext;
		}        

        //* 已经不存在伙伴节点了，在这里就直接插入当前链表即可
        buddy_insert_freed_page(pstArea, pstFreedPage); 

		//os_thread_mutex_unlock(l_hMtxMMUBuddy);
        os_exit_critical();

		return TRUE; //* 没有合并节点则结束执行

	__lblMountToUpperLink: //* 将合并后的节点挂载到上层
		i++;
		if (i < BUDDY_ARER_COUNT)
		{
			pstArea = &l_staArea[i];			
			if (!pstArea->pstFreed)
			{
				pstArea->pstFreed = pstFreedPage;
				pstFreedPage->pstNext = NULL;
                
				//os_thread_mutex_unlock(l_hMtxMMUBuddy);
                os_exit_critical();

				return TRUE;
			}

            goto __lblMerge;			         
		}        
	}    
	//os_thread_mutex_unlock(l_hMtxMMUBuddy);
    os_exit_critical();

	return TRUE; 
}

FLOAT buddy_usage(void)
{
    INT i;
    UINT unUsedPageSize = 0;

    os_critical_init();

    os_enter_critical();
    for (i = 0; i < BUDDY_ARER_COUNT; i++)
    {
        PST_BUDDY_PAGE pstNextUsedPage = l_staArea[i].pstUsed;
        while (pstNextUsedPage)
        {
            unUsedPageSize += l_staArea[i].unPageSize;
            pstNextUsedPage = pstNextUsedPage->pstNext;
        }
    }
    os_exit_critical();

    return (FLOAT)unUsedPageSize / (FLOAT)BUDDY_MEM_SIZE;
}

FLOAT buddy_usage_details(UINT *punTotalBytes, UINT *punUsedBytes, UINT *punMaxFreedPageSize, UINT *punMinFreedPageSize)
{
    INT i;
    UINT unUsedPageSize = 0;
    UINT unMaxFreedPageSize = BUDDY_PAGE_SIZE, unMinFreedPageSize = BUDDY_PAGE_SIZE;

    *punTotalBytes = BUDDY_MEM_SIZE;

    os_critical_init();

    os_enter_critical();
    for (i = 0; i < BUDDY_ARER_COUNT; i++)
    {
        PST_BUDDY_PAGE pstNextPage = l_staArea[i].pstUsed;
        while (pstNextPage)
        {
            unUsedPageSize += l_staArea[i].unPageSize;
            pstNextPage = pstNextPage->pstNext;
        }

        pstNextPage = l_staArea[i].pstFreed;
        while (pstNextPage)
        {
            if (unMaxFreedPageSize < l_staArea[i].unPageSize)
                unMaxFreedPageSize = l_staArea[i].unPageSize;
            if (unMinFreedPageSize > l_staArea[i].unPageSize)
                unMinFreedPageSize = l_staArea[i].unPageSize;
            pstNextPage = pstNextPage->pstNext;
        }
    }
    os_exit_critical();

    *punUsedBytes = unUsedPageSize;

    if (unUsedPageSize < BUDDY_MEM_SIZE)
    {
        *punMaxFreedPageSize = unMaxFreedPageSize;
        *punMinFreedPageSize = unMinFreedPageSize;
    }
    else
    {
        *punMaxFreedPageSize = 0;
        *punMinFreedPageSize = 0;
    }

    return (FLOAT)unUsedPageSize / (FLOAT)BUDDY_MEM_SIZE;
}

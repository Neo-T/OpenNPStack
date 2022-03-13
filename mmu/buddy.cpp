#include "datatype.h"
#include "sys_config.h"

#define SYMBOL_GLOBALS
#include "buddy.h"
#undef SYMBOL_GLOBALS

//* 再次把相关基础数据准备好，确保协议栈不过多占用用户的堆空间
static UCHAR l_ubaMemPool[BUDDY_MEM_SIZE];
static ST_BUDDY_AREA l_staArea[BUDDY_ARER_COUNT];
static const UINT lr_unPageCount = BUDDY_MEM_SIZE / BUDDY_PAGE_SIZE + 1;
static ST_BUDDY_PAGE l_staPage[BUDDY_MEM_SIZE / BUDDY_PAGE_SIZE + 1];
static STCB_BUDDY_PAGE_NODE l_stcbaFreePage[BUDDY_MEM_SIZE / BUDDY_PAGE_SIZE + 1];

static PST_BUDDY_PAGE GetPageNode(void)
{
	PST_BUDDY_PAGE pstPage;
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

	printf("GetPageNode()执行失败\r\n");

	return NULL;
}

static void FreePageNode(PST_BUDDY_PAGE pstPage)
{
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

	printf("FreePageNode()执行失败\r\n");
}

void buddy_init(void)
{
	INT i;
	UINT unPageSize = BUDDY_PAGE_SIZE;

	//* 存储页面控制信息的链表必须先初始化，接下来就要用到
	for (i = 0; i < lr_unPageCount; i++)
	{
		l_stcbaFreePage[i].pstPage = &l_staPage[i];
		l_stcbaFreePage[i].pstNext = &l_stcbaFreePage[i + 1];
	}
	l_stcbaFreePage[i - 1].pstNext = NULL;

	//* 清零，并把交给buddy管理的整块内存挂载到页块管理数组的最后一个单元，确保最后一个单元为最大一块连续的内存
	memset(&l_staArea, 0, sizeof(l_staArea));
	PST_BUDDY_PAGE pstPage = GetPageNode();
	pstPage->pstNext = NULL;
	pstPage->pubStart = l_ubaMemPool;
	l_staArea[BUDDY_ARER_COUNT - 1].pstNext  = pstPage;

	//* 计算并存储各页块管理单元的单个页面大小
	l_staArea[0].unPageSize = unPageSize;
	for (i = 1; i < BUDDY_ARER_COUNT; i++)
	{
		unPageSize *= 2;
		l_staArea[i].unPageSize = unPageSize;
	}
}

void *buddy_alloc(UINT unSize)
{
	INT i;
	UINT unPageSize = BUDDY_PAGE_SIZE;
	PST_BUDDY_PAGE pstPage, pstPrevPage, pstNextPage, pstFreePage;
	PST_BUDDY_AREA pstArea;
	PST_BUDDY_PAGE pstFreePage1, pstFreePage2;

	if (unSize > BUDDY_MEM_SIZE)
	{
		printf("要申请的内存超过系统支持分配的最大内存\r\n");
		return (void *)0;
	}

	//* 确定页块索引
	for (i = 1; i < BUDDY_ARER_COUNT; i++)
	{
		if (unPageSize > unSize)
		{
			i--;
			break;
		}

		unPageSize *= 2;
	}

	//* 存在有效页面则直接返回
	pstPage = l_staArea[i].pstNext;
	while (pstPage)
	{
		if (!pstPage->blIsUsed)
		{
			pstPage->blIsUsed = TRUE;
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

	printf("系统无可用内存\r\n");

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
		pstFreePage1 = GetPageNode();
		if (!pstFreePage1) //* 这属于程序BUG，理论上不应该申请不到
		{
			return (void *)0;
		}
		pstFreePage2 = GetPageNode();
		if (!pstFreePage2) //* 同上
		{
			return (void *)0;
		}
		pstFreePage1->pubStart = pstPage->pubStart;
		pstFreePage2->pubStart = pstPage->pubStart + pstArea->unPageSize;
		pstFreePage1->pubBuddyNextAddr = pstFreePage2->pubStart; 
		pstFreePage1->pubBuddyPrevAddr = NULL; 
		pstFreePage2->pubBuddyNextAddr = NULL;
		pstFreePage2->pubBuddyPrevAddr = pstFreePage1->pubStart;
		FreePageNode(pstPage); //* 归还

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
			return (void *)pstPage->pubStart;
		}
	}

	printf("系统无可用内存\r\n");

	return (void *)0;
}

void buddy_free(void *pvStart)
{
	INT i;
	PST_BUDDY_AREA pstArea; 
	PST_BUDDY_PAGE pstNextPage, pstPrevPage1, pstPrevPage2, pstFreedPage;
	UCHAR *pubBuddyAddr; 

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
				goto __lblMerge;
			}

			pstPrevPage1 = pstNextPage;
			pstNextPage = pstNextPage->pstNext;
		}
	}

	printf("释放失败\r\n");
	return;

	//* 合并操作
__lblMerge: 
	if (pstFreedPage->pubBuddyNextAddr)
		pubBuddyAddr = pstFreedPage->pubBuddyNextAddr;
	else
		pubBuddyAddr = pstFreedPage->pubBuddyPrevAddr;

	pstArea = &l_staArea[i];
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
					//* 先摘除再合并
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

				goto __lblUpperLink;
			}
		}

		pstPrevPage2 = pstNextPage;
		pstNextPage = pstNextPage->pstNext; 
	}

	return; //* 没有合并节点则结束执行

__lblUpperLink: //* 将合并后的节点挂载到上层
	i++; 
	if (i < BUDDY_ARER_COUNT)
	{
		pstArea = &l_staArea[i];
		pstNextPage = pstArea->pstNext; 
		pstPrevPage2 = NULL; 
		while (pstNextPage)
		{
			if (pstNextPage->pubBuddyNextAddr)
			{
				if (pstFreedPage->pubStart == pstNextPage->pubBuddyNextAddr) //* 说明是伙伴节点的后半部分
				{
					pstFreedPage->pstNext = pstNextPage->pstNext; 
					pstNextPage->pstNext = pstFreedPage; 

					pstFreedPage->pubBuddyNextAddr = NULL;
					pstFreedPage->pubBuddyPrevAddr = pstNextPage->pubStart; 					
					pstPrevPage1 = pstNextPage;
					
					goto __lblMerge;
				}
			}
			else
			{
				if (pstFreedPage->pubStart == pstNextPage->pubBuddyPrevAddr) //* 说明是伙伴节点的前半部分
				{
					if (pstPrevPage2)
						pstPrevPage2->pstNext = pstFreedPage;
					else
						pstArea->pstNext = pstFreedPage;
					pstFreedPage->pstNext = pstNextPage;

					pstFreedPage->pubBuddyNextAddr = pstNextPage->pubStart;
					pstFreedPage->pubBuddyPrevAddr = NULL;
					pstPrevPage1 = pstPrevPage2;
					goto __lblMerge;
				}
			}

			pstPrevPage2 = pstNextPage; 
			pstNextPage = pstNextPage->pstNext;
		}
	}

	return; 
}

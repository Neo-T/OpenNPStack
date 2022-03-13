#include "datatype.h"
#include "sys_config.h"

#define SYMBOL_GLOBALS
#include "buddy.h"
#undef SYMBOL_GLOBALS

#define TBLPOWER2SIZE_UNIT_COUNT 8
static const struct {
	UCHAR unPower;	//* 2的n次幂
	UINT unSize;	//* 单位：字节，Bytes
} lr_staPower2Size[TBLPOWER2SIZE_UNIT_COUNT] = {
	{10, 1024}
};

//* 再次把相关基础数据准备好，确保协议栈不过多占用用户的堆空间
static UCHAR l_ubaMemPool[BUDDY_MEM_SIZE];
static ST_BUDDY_AREA l_staArea[BUDDY_ARER_COUNT];
static const UINT lr_unPageCount = BUDDY_MEM_SIZE / BUDDY_PAGE_SIZE;
static ST_BUDDY_PAGE l_staPage[BUDDY_MEM_SIZE / BUDDY_PAGE_SIZE];
static STCB_BUDDY_PAGE_NODE l_stcbaFreePage[BUDDY_MEM_SIZE / BUDDY_PAGE_SIZE];
void buddy_init(void)
{
	INT i; 

	for (i = 0; i < lr_unPageCount; i++)
	{
		l_stcbaFreePage[i].pstPage = &l_staPage[i]; 
		l_stcbaFreePage[i].pstNext = &l_stcbaFreePage[i + 1];
	}
	l_stcbaFreePage[i - 1].pstNext = NULL;

	//* 清零，并把交给buddy管理的整块内存挂载到页块管理数组的最后一个单元，确保最后一个单元为最大一块连续的内存
	memset(&l_staArea, 0, sizeof(l_staArea));
	l_staPage[lr_unPageCount - 1].pstNext      = NULL; 
	l_staPage[lr_unPageCount - 1].pubStart     = l_ubaMemPool; 
	l_staArea[BUDDY_ARER_COUNT - 1].unPageSize = BUDDY_MEM_SIZE; 	
	l_staArea[BUDDY_ARER_COUNT - 1].pubStart   = l_ubaMemPool; 
	l_staArea[BUDDY_ARER_COUNT - 1].pstNext    = &l_staPage[lr_unPageCount - 1]; 
}

static PST_BUDDY_PAGE GetPageNode(void)
{
	PSTCB_BUDDY_PAGE_NODE pstNode = &l_stcbaFreePage[0]; 
	while (pstNode)
	{
		if (pstNode->pstPage)
		{
			pstNode->pstPage = NULL; 
			return pstNode->pstPage; 
		}
	}
	
	return NULL;
}

static void FreePageNode(PST_BUDDY_PAGE pstPage)
{
	PSTCB_BUDDY_PAGE_NODE pstNode = &l_stcbaFreePage[0];
	while (pstNode)
	{
		if (pstNode->pstPage)
		{
			pstNode->pstPage = NULL;
			return pstNode->pstPage;
		}
	}
}

void *buddy_alloc(UINT unSize)
{		
	INT i; 
	UINT unPageSize = BUDDY_PAGE_SIZE;
	PST_BUDDY_PAGE pstPage, pstPrevPage, pstNextPage; 

	if (unSize > BUDDY_MEM_SIZE)
		return (void *)0;

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
		pstNextPage = l_staArea[i - 1].pstNext;
		while (TRUE)
		{
			if (!pstNextPage)
			{
				pstNextPage = pstPage->pubStart;
			}
		}
	}

	//* 执行到这里没有可用页块，需要从更大的页块区分裂出一块来使用
	i = BUDDY_ARER_COUNT - 1;
	for (; i >= 0; i--)
	{		
		if (unSize > l_staArea[i].unPageSize / 2)
		{
			if (l_staArea[i].pstNext)
			{
				l_staArea[i].pstNext = l_staArea[i].pstNext->pstNext; //* 指向下一个可用节点
				return (void *)l_staArea[i].pstNext->pubStart;
			}
			else
				break;
		}
		else
		{
			if (l_staArea[i].pstNext)
			{

			}

		}

		

		if (l_staArea[i].pstNext)
		{
			if (unSize > l_staArea[i].unPageSize / 2)
			{
				l_staArea[i].pstNext = l_staArea[i].pstNext->pstNext; //* 指向下一个可用节点
				return (void *)l_staArea[i].pstNext->pubStart;
			}
		}
		else
		{

		}

		if (unSize > l_staArea[i].unPageSize / 2)
		{
			if (l_staArea[i].pstNext)
				return (void *)l_staArea[i].pstNext->pubStart;
			else
				return NULL;
		}

		//* 如果为空，则需要从前一个内存块分裂出一块给当前内存
		if (l_staArea[i].pstNext)
		{

		}
		else
		{
			if()
		}
	}

	while (TRUE)
	{
		
	}

	return (void *)0;
}
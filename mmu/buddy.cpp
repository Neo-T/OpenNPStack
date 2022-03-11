#include "datatype.h"
#include "sys_config.h"

#define SYMBOL_GLOBALS
#include "buddy.h"
#undef SYMBOL_GLOBALS

static UCHAR l_ubaMemPool[BUDDY_MEM_SIZE];
static ST_BUDDY_AREA l_staArea[BUDDY_ARER_COUNT];
static const UINT lr_unPageCount = (BUDDY_ARER_COUNT - 1) * 2 + 1;
static ST_BUDDY_PAGE l_staPage[lr_unPageCount];

void buddy_init(void)
{
	//* 清零，并把交给buddy管理的整块内存挂载到页块管理数组的最后一个单元，确保最后一个单元为最大一块连续的内存
	memset(&l_staArea, 0, sizeof(l_staArea));
	l_staPage[lr_unPageCount - 1].pstNext = NULL; 
	l_staPage[lr_unPageCount - 1].pubStart = l_ubaMemPool; 
	l_staArea[BUDDY_ARER_COUNT - 1].unPageSize = BUDDY_MEM_SIZE; 	
	l_staArea[BUDDY_ARER_COUNT - 1].pstNext = &l_staPage[lr_unPageCount - 1]; 
}

void buddy_alloc(UINT unSize)
{
	
}
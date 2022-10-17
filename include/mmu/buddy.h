/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 采用Buddy算法实现的内存管理单元模块用到的相关数据结构及宏定义文件
 *
 * Neo-T, 创建于2022.03.11 14:28
 *
 */
#ifndef BUDDY_H
#define BUDDY_H

#ifdef SYMBOL_GLOBALS
	#define BUDDY_EXT
#else
	#define BUDDY_EXT extern
#endif //* SYMBOL_GLOBALS

typedef struct _ST_BUDDY_PAGE_ { //* 具有相同页面大小的页面链表节点的基本数据结构
    struct _ST_BUDDY_PAGE_ *pstNext;
	BOOL blIsUsed; 
    UCHAR *pubStart;     
} ST_BUDDY_PAGE, *PST_BUDDY_PAGE; 

typedef struct _STCB_BUDDY_PAGE_NODE_ { //* 页面控制块链表节点
	struct _STCB_BUDDY_PAGE_NODE_  *pstNext;
	PST_BUDDY_PAGE pstPage; 
} STCB_BUDDY_PAGE_NODE, *PSTCB_BUDDY_PAGE_NODE;

typedef struct _ST_BUDDY_AREA_ { //* 具有相同页面大小的页块数组单元的基本数据结构
    PST_BUDDY_PAGE pstNext; 
    UINT unPageSize;	//* 该内存块单个页面大小
} ST_BUDDY_AREA, *PST_BUDDY_AREA; 

BUDDY_EXT BOOL buddy_init(EN_ONPSERR *penErr);				 //* buddy模块初始化函数
BUDDY_EXT void buddy_uninit(void);	                                 //* buddy模块去初始化函数
BUDDY_EXT void *buddy_alloc(UINT unSize, EN_ONPSERR *penErr); //* 只有没有可用的内存块了才会返回NULL，其它情况都会返回一个合适大小的内存块
BUDDY_EXT BOOL buddy_free(void *pvStart);							 //* 释放由buddy_alloc分配的内存
#endif

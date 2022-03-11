/* buddy.h
 *
 * 采用Buddy算法实现的内存管理单元模块用到的相关数据结构及宏定义文件
 *
 * Neo-T, 创建于2022.03.11 14:28
 * 版本: 1.0
 *
 */
#ifndef BUDDY_H
#define BUDDY_H
typedef struct _ST_BUDDY_PAGE_ { //* 具有相同页面大小的页面链表节点的基本数据结构
    struct _ST_BUDDY_PAGE_ *pstNext;     
    UCHAR *pubStart;     
} ST_BUDDY_PAGE, *PST_BUDDY_PAGE; 

typedef struct _ST_BUDDY_AREA_ { //* 具有相同页面大小的页块数组单元的基本数据结构
    PST_BUDDY_PAGE pstNext; 
    UINT unPageSize; 
    UCHAR ubMap;  //* 标识两个伙伴页面的使用情况：0，两个均空闲或均在使用；1，一个在使用另一个空闲
} ST_BUDDY_AREA, *PST_BUDDY_AREA; 
#endif
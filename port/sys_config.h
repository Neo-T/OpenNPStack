/* sys_config.h
 *
 * 系统配置头文件，用户可根据实际情况对协议栈进行裁剪、参数配置等工作
 *
 * Neo-T, 创建于2022.03.11 14:45
 * 版本: 1.0
 *
 */
#ifndef SYS_CONFIG_H
#define SYS_CONFIG_H

//* 内存管理单元(mmu)相关配置项
//* =======================================================
#define BUDDY_PAGE_SIZE  64     //* 系统能够分配的最小页面大小，如果小内存用量大则页大小调小一些以减少内存浪费，反之则大些，页大小必须是2的整数次幂
#define BUDDY_ARER_COUNT 8      //* 指定buddy算法管理的内存块数组单元数量
#define BUDDY_MEM_SIZE   8192   //* buddy算法管理的内存总大小，其值由BUDDY_PAGE_SIZE、BUDDY_ARER_COUNT两个宏计算得到：
                                //* 64*(2^(8 - 1))，64乘以2的(8 - 1)次方，即BUDDY_MEM_SIZE = BUDDY_PAGE_SIZE * (2 ^ (BUDDY_ARER_COUNT - 1))
//* =======================================================

#endif
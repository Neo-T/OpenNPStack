/* ctype.h
 *
 * C语言通用数据类型定义头文件
 *
 * Marsstory99, 创建于2011.10.26 10：32 
 * 版本: 0.1
 *
 */
#ifndef CTYPE_H
#define CTYPE_H

#include <cstddef>

#define	PACKED __attribute__((packed))

//* 系统常用数据类型定义(不同的编译器版本，各数据类型的位宽亦不同，请根据后面注释选择相同位宽的类型定义)
typedef unsigned long long 	ULONGLONG;		//* 64位无符号长整型
typedef long long 			LONGLONG;	  	//* 64位有符号长整型

typedef signed long			LONG;			//* 32位的有符号长整型
typedef unsigned long		ULONG;			//* 32位的无符号长整型
typedef float 				FLOAT;			//* 32位的浮点型
typedef double              DOUBLE;			//* 64位的双精度浮点型
typedef signed int 			INT;			//* 32位的有符号整型
typedef unsigned int		UINT;			//* 32位的无符号整型
typedef signed short		SHORT;			//* 16位的有符号短整型
typedef unsigned short		USHORT;			//* 16位的无符号短整型
typedef char				CHAR;			//* 8位有符号字节型，为了避免与WIN平台下的类型定义冲突故多加了个S
typedef	unsigned char		UCHAR;			//* 8位无符号字节型
typedef unsigned int		BOOL;

typedef INT HANDLE;			//* 设备操作句柄
#define INVALID_HANDLE 	-1	//* 无效的设备操作句柄

#define TRUE 	1
#define FALSE 	0

#define INVALID_DOUBLE_VALUE	-424242424242.42

typedef union _UN_FLOAT_
{
	FLOAT flVal;		//* 4个字节浮点型变量
	UCHAR ubaByte[4];	//* 字节数组，元素为4
} UN_FLOAT, *PUN_FLOAT;

//* 大小端转换宏
#define ENDIAN_BIG_UINT(n)		((((n) & 0xff)<<24) | (((n) & 0xff00) << 8) | (((n) & 0xff0000) >> 8) | (((n) & 0xff000000) >> 24))
#define ENDIAN_BIG_USHORT(n)	((((n) & 0xff)<<8) | (((n) & 0xff00)>>8))

#endif


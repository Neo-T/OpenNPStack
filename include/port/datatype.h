/* datatype.h
 *
 * 通用数据类型定义
 *
 * Neo-T, 创建于2022.03.11 13:39 
 * 版本: 1.0
 *
 */
#ifndef DATATYPE_H
#define DATATYPE_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define	PACKED __attribute__((packed))
#define PACKED_FIELD(x) PACKED x
#define PACKED_BEGIN 
#define PACKED_END

//* 系统常用数据类型定义(不同的编译器版本，各数据类型的位宽亦不同，请根据后面注释选择相同位宽的类型定义)
typedef unsigned long long 	ULONGLONG;  //* 64位无符号长整型
typedef long long 			LONGLONG;   //* 64位有符号长整型
typedef signed long			LONG;       //* 32位的有符号长整型
typedef unsigned long		ULONG;      //* 32位的无符号长整型
typedef float 				FLOAT;      //* 32位的浮点型
typedef double              DOUBLE;     //* 64位的双精度浮点型
typedef signed int 			INT;        //* 32位的有符号整型
typedef unsigned int		UINT;       //* 32位的无符号整型
typedef signed short		SHORT;      //* 16位的有符号短整型
typedef unsigned short		USHORT;     //* 16位的无符号短整型
typedef char				CHAR;       //* 8位有符号字节型
typedef	unsigned char		UCHAR;      //* 8位无符号字节型
typedef unsigned int		BOOL;       //* bool型变量

#ifndef NULL
typedef NULL (void *)0
#endif

#ifndef TRUE
#define TRUE 	1
#endif

#ifndef FALSE
#define FALSE 	0
#endif

#endif


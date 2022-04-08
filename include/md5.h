/* md5.h
 *
 * 提供md5摘要计算相关的功能函数
 *
 * Neo-T, 创建于2022.03.21 16:48
 * 版本: 1.0
 *
 */
#ifndef MD5_H
#define MD5_H

#ifdef SYMBOL_GLOBALS
	#define MD5_EXT
#else
	#define MD5_EXT extern
#endif //* SYMBOL_GLOBALS

 //* MD5摘要值结构体
typedef struct _ST_MD5VAL_ {
	UINT a;
	UINT b;
	UINT c;
	UINT d;
} ST_MD5VAL, *PST_MD5VAL;

MD5_EXT ST_MD5VAL md5(UCHAR *pubData, UINT unDataBytes);

#endif

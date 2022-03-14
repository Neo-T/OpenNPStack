/* errors.h
 *
 * 错误类型定义
 *
 * Neo-T, 创建于2022.03.14 17:14
 * 版本: 1.0
 *
 */
#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
	ERRNO = 0, //* 没有发生任何错误
	
} EN_ERROR_CODE;

typedef struct _ST_ERROR_ {
	EN_ERROR_CODE enCode; 
	CHAR szDesc[128];
} ST_ERROR, *PST_ERROR;

#endif


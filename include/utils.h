/* utils.h
 *
 * 协议栈用到的相关功能函数在此文件声明、定义
 *
 * Neo-T, 创建于2022.03.21 16:05
 * 版本: 1.0
 *
 */
#ifndef UTILS_H
#define UTILS_H

#ifdef SYMBOL_GLOBALS
	#define UTILS_EXT
#else
	#define UTILS_EXT extern
#endif //* SYMBOL_GLOBALS

 //* 大小端转换宏
#define ENDIAN_CONVERTER_UINT(n)    ((((n) & 0xFF) << 24) | (((n) & 0xFF00) << 8) | (((n) & 0xFF0000) >> 8) | (((n) & 0xFF000000) >> 24))
#define ENDIAN_CONVERTER_USHORT(n)	((((n) & 0xFF) << 8)  | (((n) & 0xFF00) >> 8))

#if !(defined(__linux__) || defined(__linux)) && !(defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)) && !(defined(WIN64) || defined(_WIN64) || defined(__WIN64__))
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

UTILS_EXT CHAR *mem_char(CHAR *pszMem, CHAR ch, UINT unMemSize);
UTILS_EXT CHAR *mem_str(CHAR *pszMem, CHAR *pszStr, UINT unStrSize, UINT unMemSize); 
UTILS_EXT USHORT tcpip_checksum(USHORT *pusData, INT nDataBytes);
UTILS_EXT void snprintf_hex(const UCHAR *pubHexData, USHORT usHexDataLen, CHAR *pszDstBuf, UINT unDstBufSize, BOOL blIsSeparateWithSpace); 

#if SUPPORT_PRINTF
UTILS_EXT void printf_hex(const UCHAR *pubHex, USHORT usHexDataLen, UCHAR ubBytesPerLine);
UTILS_EXT void printf_hex_ext(SHORT sBufListHead, UCHAR ubBytesPerLine);
#endif

#endif
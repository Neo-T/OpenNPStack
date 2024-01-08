/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 */
#include "port/datatype.h"

#define SYMBOL_GLOBALS
#include "onps_md5.h"
#undef SYMBOL_GLOBALS

//* F, G, H and I are basic MD5 functions.
#define F(x, y, z) (((*x) & (*y)) | ((~(*x)) & (*z)))
#define G(x, y, z) (((*x) & (*z)) | ((*y) & (~(*z))))
#define H(x, y, z) ((*x) ^ (*y) ^ (*z))
#define I(x, y, z) ((*y) ^ ((*x) | (~(*z))))

//* ROTATE_LEFT rotates x left n bits.
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

//* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.Rotation is separate from addition to prevent recomputation.
#define FF(a, b, c, d, x, s, ac) { \
	(*a) += F ((b), (c), (d)) + (x) + (UINT)(ac); \
	(*a) = ROTATE_LEFT ((*a), (s)); \
	(*a) += (*b); \
}
#define GG(a, b, c, d, x, s, ac) { \
	(*a) += G ((b), (c), (d)) + (x) + (UINT)(ac); \
	(*a) = ROTATE_LEFT ((*a), (s)); \
	(*a) += (*b); \
}
#define HH(a, b, c, d, x, s, ac) { \
	(*a) += H ((b), (c), (d)) + (x) + (UINT)(ac); \
	(*a) = ROTATE_LEFT ((*a), (s)); \
	(*a) += (*b); \
}
#define II(a, b, c, d, x, s, ac) { \
	(*a) += I ((b), (c), (d)) + (x) + (UINT)(ac); \
	(*a) = ROTATE_LEFT ((*a), (s)); \
	(*a) += (*b); \
}

//* Constants for MD5 Transform routine.
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

ST_MD5VAL onps_md5(UCHAR *pubData, UINT unDataBytes)
{
	ST_MD5VAL stVal = { 0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476 };
	UINT *punA, *punB, *punC, *punD, *punX;
	UINT unA, unB, unC, unD;
	UINT unHasReadBytes = 0;
	UCHAR ubaDataBlock[128];
	UCHAR *pubReadData;
	UINT unCalCount, i;

	punA = &stVal.a;
	punB = &stVal.b;
	punC = &stVal.c;
	punD = &stVal.d;

	do {
		UINT unCurReadBytes = unDataBytes - unHasReadBytes > 64 ? 64 : unDataBytes - unHasReadBytes;
		if (unCurReadBytes == 64)
		{
			pubReadData = pubData + unHasReadBytes;
			unHasReadBytes += 64;
			unCalCount = 64;
		}
		else
		{
			UINT k = unCurReadBytes;
			memcpy(ubaDataBlock, pubData + unHasReadBytes, unCurReadBytes);
			ubaDataBlock[k++] = 0x80;
			for (; k % 64 != 56; k++)
				ubaDataBlock[k] = 0x00;

			unHasReadBytes += unCurReadBytes;
			*(UINT *)(ubaDataBlock + k) = unHasReadBytes << 3;
			k += 4;
			*(UINT *)(ubaDataBlock + k) = unHasReadBytes >> 29;
			k += 4;

			pubReadData = ubaDataBlock;
			unCalCount = k;
		}

		//* 开始计算MD5
		//* ================================================================
		for (i = 0; i<unCalCount; i += 64)
		{
			punX = (UINT *)(pubReadData + i);

			unA = *punA;
			unB = *punB;
			unC = *punC;
			unD = *punD;

			//* Round 1
			FF(punA, punB, punC, punD, punX[0], S11, 0xd76aa478);	//* 1 
			FF(punD, punA, punB, punC, punX[1], S12, 0xe8c7b756);	//* 2 
			FF(punC, punD, punA, punB, punX[2], S13, 0x242070db);	//* 3 
			FF(punB, punC, punD, punA, punX[3], S14, 0xc1bdceee);	//* 4 
			FF(punA, punB, punC, punD, punX[4], S11, 0xf57c0faf);	//* 5 
			FF(punD, punA, punB, punC, punX[5], S12, 0x4787c62a);	//* 6 
			FF(punC, punD, punA, punB, punX[6], S13, 0xa8304613);	//* 7 
			FF(punB, punC, punD, punA, punX[7], S14, 0xfd469501);	//* 8 
			FF(punA, punB, punC, punD, punX[8], S11, 0x698098d8);	//* 9 
			FF(punD, punA, punB, punC, punX[9], S12, 0x8b44f7af);	//* 10 
			FF(punC, punD, punA, punB, punX[10], S13, 0xffff5bb1);	//* 11 
			FF(punB, punC, punD, punA, punX[11], S14, 0x895cd7be);	//* 12 
			FF(punA, punB, punC, punD, punX[12], S11, 0x6b901122);	//* 13 
			FF(punD, punA, punB, punC, punX[13], S12, 0xfd987193);	//* 14 
			FF(punC, punD, punA, punB, punX[14], S13, 0xa679438e);	//* 15 
			FF(punB, punC, punD, punA, punX[15], S14, 0x49b40821);	//* 16 

			//* Round 2
			GG(punA, punB, punC, punD, punX[1], S21, 0xf61e2562);	//* 17 
			GG(punD, punA, punB, punC, punX[6], S22, 0xc040b340);	//* 18 
			GG(punC, punD, punA, punB, punX[11], S23, 0x265e5a51);	//* 19 
			GG(punB, punC, punD, punA, punX[0], S24, 0xe9b6c7aa);	//* 20 
			GG(punA, punB, punC, punD, punX[5], S21, 0xd62f105d);	//* 21 
			GG(punD, punA, punB, punC, punX[10], S22, 0x02441453);	//* 22 
			GG(punC, punD, punA, punB, punX[15], S23, 0xd8a1e681);	//* 23 
			GG(punB, punC, punD, punA, punX[4], S24, 0xe7d3fbc8);	//* 24 
			GG(punA, punB, punC, punD, punX[9], S21, 0x21e1cde6);	//* 25 
			GG(punD, punA, punB, punC, punX[14], S22, 0xc33707d6);	//* 26 
			GG(punC, punD, punA, punB, punX[3], S23, 0xf4d50d87);	//* 27 
			GG(punB, punC, punD, punA, punX[8], S24, 0x455a14ed);	//* 28 
			GG(punA, punB, punC, punD, punX[13], S21, 0xa9e3e905);	//* 29 
			GG(punD, punA, punB, punC, punX[2], S22, 0xfcefa3f8);	//* 30 
			GG(punC, punD, punA, punB, punX[7], S23, 0x676f02d9);	//* 31 
			GG(punB, punC, punD, punA, punX[12], S24, 0x8d2a4c8a);	//* 32 

			//* Round 3
			HH(punA, punB, punC, punD, punX[5], S31, 0xfffa3942);	//* 33 
			HH(punD, punA, punB, punC, punX[8], S32, 0x8771f681);	//* 34 
			HH(punC, punD, punA, punB, punX[11], S33, 0x6d9d6122);	//* 35 
			HH(punB, punC, punD, punA, punX[14], S34, 0xfde5380c);	//* 36 
			HH(punA, punB, punC, punD, punX[1], S31, 0xa4beea44);	//* 37 
			HH(punD, punA, punB, punC, punX[4], S32, 0x4bdecfa9);	//* 38 
			HH(punC, punD, punA, punB, punX[7], S33, 0xf6bb4b60);	//* 39 
			HH(punB, punC, punD, punA, punX[10], S34, 0xbebfbc70);	//* 40 
			HH(punA, punB, punC, punD, punX[13], S31, 0x289b7ec6);	//* 41 
			HH(punD, punA, punB, punC, punX[0], S32, 0xeaa127fa);	//* 42 
			HH(punC, punD, punA, punB, punX[3], S33, 0xd4ef3085);	//* 43 
			HH(punB, punC, punD, punA, punX[6], S34, 0x04881d05);	//* 44 
			HH(punA, punB, punC, punD, punX[9], S31, 0xd9d4d039);	//* 45 
			HH(punD, punA, punB, punC, punX[12], S32, 0xe6db99e5);	//* 46 
			HH(punC, punD, punA, punB, punX[15], S33, 0x1fa27cf8);	//* 47 
			HH(punB, punC, punD, punA, punX[2], S34, 0xc4ac5665);	//* 48 

			//* Round 4
			II(punA, punB, punC, punD, punX[0], S41, 0xf4292244);	//* 49 
			II(punD, punA, punB, punC, punX[7], S42, 0x432aff97);	//* 50 
			II(punC, punD, punA, punB, punX[14], S43, 0xab9423a7);	//* 51 
			II(punB, punC, punD, punA, punX[5], S44, 0xfc93a039);	//* 52 
			II(punA, punB, punC, punD, punX[12], S41, 0x655b59c3);	//* 53 
			II(punD, punA, punB, punC, punX[3], S42, 0x8f0ccc92);	//* 54 
			II(punC, punD, punA, punB, punX[10], S43, 0xffeff47d);	//* 55 
			II(punB, punC, punD, punA, punX[1], S44, 0x85845dd1);	//* 56 
			II(punA, punB, punC, punD, punX[8], S41, 0x6fa87e4f);	//* 57 
			II(punD, punA, punB, punC, punX[15], S42, 0xfe2ce6e0);	//* 58 
			II(punC, punD, punA, punB, punX[6], S43, 0xa3014314);	//* 59 
			II(punB, punC, punD, punA, punX[13], S44, 0x4e0811a1);	//* 60 
			II(punA, punB, punC, punD, punX[4], S41, 0xf7537e82);	//* 61 
			II(punD, punA, punB, punC, punX[11], S42, 0xbd3af235);	//* 62 
			II(punC, punD, punA, punB, punX[2], S43, 0x2ad7d2bb);	//* 63 
			II(punB, punC, punD, punA, punX[9], S44, 0xeb86d391);	//* 64 

			//* Add the original values
			*punA += unA;
			*punB += unB;
			*punC += unC;
			*punD += unD;
		}
		//* ================================================================		
	} while (unHasReadBytes < unDataBytes);

	return stVal;
}


#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "mmu/buf_list.h"

#define SYMBOL_GLOBALS
#include "utils.h"
#undef SYMBOL_GLOBALS

CHAR *mem_char(CHAR *pszMem, CHAR ch, UINT unMemSize)
{
	CHAR *pszNext = pszMem;
	UINT i;

	for (i = 0; i < unMemSize; i++)
	{
		if (*(pszNext + i) == ch)
			return (pszNext + i);
	}

	return NULL;
}

CHAR *mem_str(CHAR *pszMem, CHAR *pszStr, UINT unStrSize, UINT unMemSize)
{
	CHAR *pszNext = pszMem;

	pszNext = mem_char(pszNext, pszStr[0], unMemSize);
	while (pszNext != NULL)
	{
		if (strncmp((char *)pszNext, (char *)pszStr, unStrSize) == 0)
			return pszNext; 

		pszNext += 1;
		pszNext = mem_char(pszNext, (CHAR)pszStr[0], (pszMem + unMemSize) - pszNext);
	}

	return NULL;
}

USHORT tcpip_checksum(USHORT *pusData, INT nDataBytes)
{
	ULONG ulChecksum = 0;

	while (nDataBytes > 1)
	{
		ulChecksum += *pusData++;
		nDataBytes -= sizeof(USHORT);
	}

	if (nDataBytes)
	{
		ulChecksum += *(UCHAR*)pusData;
	}
	ulChecksum = (ulChecksum >> 16) + (ulChecksum & 0xffff);
	ulChecksum += (ulChecksum >> 16);

	return (USHORT)(~ulChecksum);
}

void snprintf_hex(const UCHAR *pubHexData, USHORT usHexDataLen, CHAR *pszDstBuf, UINT unDstBufSize, BOOL blIsSeparateWithSpace)
{
	UINT i, unFormatBytes = 0;

	if (!usHexDataLen || !unDstBufSize || unDstBufSize < 4)
		return;

	pszDstBuf[0] = 0;

	sprintf(pszDstBuf, "%02X", pubHexData[0]);
	unFormatBytes = 2;
	for (i = 1; i < usHexDataLen; i++)
	{
		CHAR szHex[sizeof(UCHAR) * 2 + 2];
		if (blIsSeparateWithSpace)
		{
			sprintf(szHex, " %02X", pubHexData[i]);
			unFormatBytes += 3;
		}
		else
		{
			sprintf(szHex, "%02X", pubHexData[i]);
			unFormatBytes += 2;
		}

		if (unFormatBytes < unDstBufSize)
			strcat(pszDstBuf, szHex);
		else
			break;
	}

	pszDstBuf[unFormatBytes] = 0;
}

#if SUPPORT_PRINTF
void printf_hex(const UCHAR *pubHex, USHORT usHexDataLen, UCHAR ubBytesPerLine)
{
	INT i;

	printf("%02X", pubHex[0]);
	for (i = 1; i < usHexDataLen; i++)
	{
		if (i % (INT)ubBytesPerLine)
			printf(" ");
		else
			printf("\r\n");
		printf("%02X", pubHex[i]);
	}

	printf("\r\n");
}

void printf_hex_ext(SHORT sBufListHead, UCHAR ubBytesPerLine)
{
	SHORT sNextNode = sBufListHead;
	UCHAR *pubData;
	USHORT usDataLen;
	INT i = 0;
	while (NULL != (pubData = (UCHAR *)buf_list_get_next_node(&sNextNode, &usDataLen)))
	{
		INT k;		
		for (k = 0; k < (INT)usDataLen; i++, k++)
		{
			if (i >= ubBytesPerLine)
			{
				i = 0;
				printf("\r\n"); 
			}

			if (i)
				printf(" %02X", pubData[k]);							
			else			
				printf("%02X", pubData[k]);				
		}
	}
	printf("\r\n"); 
}
#endif

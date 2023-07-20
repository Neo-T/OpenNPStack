/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "ip/ip_frame.h"
#define SYMBOL_GLOBALS
#include "onps_utils.h"
#undef SYMBOL_GLOBALS
#include "netif/netif.h"

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

USHORT tcpip_checksum_ext(SHORT sBufListHead)
{
    ULONG ulChecksum = 0;

    SHORT sNextNode = sBufListHead;
    UCHAR *pubData;
    USHORT usDataLen;
    USHORT usData = 0; 
    CHAR bState = 0;

__lblGetNextNode:
    pubData = (UCHAR *)buf_list_get_next_node(&sNextNode, &usDataLen);
    if (NULL == pubData)
    {
        ulChecksum += usData; 

        ulChecksum = (ulChecksum >> 16) + (ulChecksum & 0xffff);
        ulChecksum += (ulChecksum >> 16);

        return (USHORT)(~ulChecksum);
    }
    
    USHORT i;     
    for (i = 0; i < usDataLen; i++)
    {
        switch (bState)
        {
        case 0:             
            ((UCHAR *)&usData)[0] = pubData[i]; 
            bState = 1; 
            break; 

        case 1:
            ((UCHAR *)&usData)[1] = pubData[i];
            ulChecksum += usData;
            usData = 0;
            bState = 0;
            break;
        }
    }

    goto __lblGetNextNode; 
}

USHORT tcpip_checksum_ipv4(in_addr_t unSrcAddr, in_addr_t unDstAddr, USHORT usPayloadLen, UCHAR ubProto, SHORT sBufListHead, EN_ONPSERR *penErr)
{
	//* 填充用于校验和计算的ip伪报头
	ST_IP_PSEUDOHDR stPseudoHdr;
	stPseudoHdr.unSrcAddr = unSrcAddr;
	stPseudoHdr.unDstAddr = unDstAddr;
	stPseudoHdr.ubMustBeZero = 0;
	stPseudoHdr.ubProto = ubProto;
	stPseudoHdr.usPacketLen = htons(usPayloadLen); 	
	SHORT sPseudoHdrNode = buf_list_get_ext((UCHAR *)&stPseudoHdr, (UINT)sizeof(ST_IP_PSEUDOHDR), penErr);
	if (sPseudoHdrNode < 0)
		return 0;
	buf_list_put_head(&sBufListHead, sPseudoHdrNode);

	//* 计算校验和
	USHORT usChecksum = tcpip_checksum_ext(sBufListHead);
	
	//* 释放伪报头
	buf_list_free_head(&sBufListHead, sPseudoHdrNode);

	return usChecksum; 
}

USHORT tcpip_checksum_ipv4_ext(in_addr_t unSrcAddr, in_addr_t unDstAddr, UCHAR ubProto, UCHAR *pubPayload, USHORT usPayloadLen, EN_ONPSERR *penErr)
{
	//* 把完整的udp报文放到buf list链表以便计算udp校验和确保收到的udp报文正确        
	SHORT sBufListHead = -1;
	SHORT sPayloadNode = buf_list_get_ext(pubPayload, (UINT)usPayloadLen, penErr); 
	if (sPayloadNode < 0)
		return 0; 
	buf_list_put_head(&sBufListHead, sPayloadNode);  

	USHORT usChecksum = tcpip_checksum_ipv4(unSrcAddr, unDstAddr, usPayloadLen, ubProto, sBufListHead, penErr); 
	buf_list_free(sPayloadNode); 

	return usChecksum; 
}

#if SUPPORT_IPV6
USHORT tcpip_checksum_ipv6(UCHAR ubaSrcAddr[16], UCHAR ubaDstAddr[16], UINT unPayloadLen, UCHAR ubProto, SHORT sBufListHead, EN_ONPSERR *penErr)
{
	//* 填充用于校验和计算的ipv6伪报头
	ST_IPv6_PSEUDOHDR stPseudoHdr;
	memcpy(stPseudoHdr.ubaSrcIpv6, ubaSrcAddr, 16);
	memcpy(stPseudoHdr.ubaDstIpv6, ubaDstAddr, 16);
	stPseudoHdr.unIpv6PayloadLen = htonl(unPayloadLen);
	stPseudoHdr.ubaMustBeZero[0] = stPseudoHdr.ubaMustBeZero[1] = stPseudoHdr.ubaMustBeZero[2] = 0;
	stPseudoHdr.ubProto = ubProto;
	SHORT sPseudoHdrNode = buf_list_get_ext((UCHAR *)&stPseudoHdr, (UINT)sizeof(ST_IPv6_PSEUDOHDR), penErr); 
	if (sPseudoHdrNode < 0)			
		return 0;	
	buf_list_put_head(&sBufListHead, sPseudoHdrNode);

	//* 计算校验和
	USHORT usChecksum = tcpip_checksum_ext(sBufListHead);	

	//* 释放伪报头
	buf_list_free_head(&sBufListHead, sPseudoHdrNode); 

	return usChecksum; 
}

USHORT tcpip_checksum_ipv6_ext(UCHAR ubaSrcAddr[16], UCHAR ubaDstAddr[16], UCHAR ubProto, UCHAR *pubPayload, UINT unPayloadLen, EN_ONPSERR *penErr)
{
	//* 把完整的udp报文放到buf list链表以便计算udp校验和确保收到的udp报文正确        
	SHORT sBufListHead = -1;
	SHORT sPayloadNode = buf_list_get_ext(pubPayload, unPayloadLen, penErr);
	if (sPayloadNode < 0)
		return 0; 
	buf_list_put_head(&sBufListHead, sPayloadNode); 

	USHORT usChecksum = tcpip_checksum_ipv6(ubaSrcAddr, ubaDstAddr, (USHORT)unPayloadLen, ubProto, sBufListHead, penErr);
	buf_list_free(sPayloadNode);

	return usChecksum;
}
#endif

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
    if (!usHexDataLen)
        return; 
	
	printf("%02X", pubHex[0]);
    INT i;
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

PST_SLINKEDLIST_NODE sllist_get_node(PST_SLINKEDLIST *ppstSLList)
{
    if (NULL == *ppstSLList)
        return NULL; 

    PST_SLINKEDLIST_NODE pstNode = *ppstSLList;    
    *ppstSLList = (*ppstSLList)->pstNext; 

    pstNode->pstNext = NULL;
    return pstNode; 
}

PST_SLINKEDLIST_NODE sllist_get_tail_node(PST_SLINKEDLIST *ppstSLList)
{
    if (NULL == *ppstSLList)
        return NULL;

    PST_SLINKEDLIST_NODE pstNextNode = *ppstSLList;
    PST_SLINKEDLIST_NODE pstPrevNode = NULL;
    while (pstNextNode)
    {
        if(NULL == pstNextNode->pstNext) //* 到了尾部节点了
        { 
            if (pstPrevNode)
                pstPrevNode->pstNext = NULL;
            else
                *ppstSLList = NULL; 
            pstNextNode->pstNext = NULL; 
            return pstNextNode; 
        }

        pstPrevNode = pstNextNode;
        pstNextNode = pstNextNode->pstNext; 
    }

    return NULL; 
}

void sllist_del_node(PST_SLINKEDLIST *ppstSLList, PST_SLINKEDLIST_NODE pstNode)
{
    PST_SLINKEDLIST_NODE pstNextNode = *ppstSLList;
    PST_SLINKEDLIST_NODE pstPrevNode = NULL;
    while (pstNextNode)
    {
        //* 节点地址相等则意味着找到了要删除的节点在链表中的链接位置
        if (pstNextNode == pstNode)
        {
            if (pstPrevNode)
                pstPrevNode->pstNext = pstNextNode->pstNext; 
            else
                *ppstSLList = pstNextNode->pstNext;

            break; 
        }

        pstPrevNode = pstNextNode;
        pstNextNode = pstNextNode->pstNext;
    }
}

void sllist_del_node_ext(PST_SLINKEDLIST *ppstSLList, void *pvData)
{
    PST_SLINKEDLIST_NODE pstNextNode = *ppstSLList;
    PST_SLINKEDLIST_NODE pstPrevNode = NULL;
    while (pstNextNode)
    {
        //* 节点携带的数据地址相等则意味着找到了要删除的节点在链表中的链接位置
        if (pstNextNode->uniData.ptr == pvData)
        {
            if (pstPrevNode)
                pstPrevNode->pstNext = pstNextNode->pstNext;
            else
                *ppstSLList = pstNextNode->pstNext;

            break;
        }

        pstPrevNode = pstNextNode;
        pstNextNode = pstNextNode->pstNext;
    }
}

void sllist_put_node(PST_SLINKEDLIST *ppstSLList, PST_SLINKEDLIST_NODE pstNode)
{
    pstNode->pstNext = *ppstSLList; 
    *ppstSLList = pstNode; 
}

void sllist_put_tail_node(PST_SLINKEDLIST *ppstSLList, PST_SLINKEDLIST_NODE pstNode)
{
    if (NULL == *ppstSLList)
    {
        *ppstSLList = pstNode;
        pstNode->pstNext = NULL; 
        return; 
    }

    PST_SLINKEDLIST_NODE pstNextNode = *ppstSLList;
    PST_SLINKEDLIST_NODE pstPrevNode = NULL;
    while (pstNextNode)
    {
        pstPrevNode = pstNextNode;
        pstNextNode = pstNextNode->pstNext;
    }

    //* 一旦执行到这里尾部节点一定不为NULL
    pstPrevNode->pstNext = pstNode; 
    pstNode->pstNext = NULL; 
}

CHAR array_linked_list_get_index(void *pvUnit, void *pvArray, UCHAR ubUnitSize, CHAR bUnitNum)
{
	CHAR i;
	for (i = 0; i < bUnitNum; i++)
	{		
		if ((UCHAR *)pvUnit == ((UCHAR *)pvArray + i * ubUnitSize))
			return i;
	}

	return -1;
}

void *array_linked_list_get(CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit, CHAR *pbUnitIdx)
{
    void *pvUnit;
    
    if (*pbListHead < 0)            
        return NULL;     

    if (pbUnitIdx)
        *pbUnitIdx = *pbListHead;

    pvUnit = (UCHAR *)pvArray + (*pbListHead) * ubUnitSize;
    *pbListHead = *((CHAR *)pvArray + (*pbListHead) * ubUnitSize + bOffsetNextUnit);

    return pvUnit;
}

void *array_linked_list_get_safe(CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit, CHAR *pbUnitIdx)
{
	void *pvUnit;

	os_critical_init();
	os_enter_critical();
	{
		if (*pbListHead < 0)
		{
			os_exit_critical(); 
			return NULL;
		}

		if (pbUnitIdx)
			*pbUnitIdx = *pbListHead; 
		 
		pvUnit = (UCHAR *)pvArray + (*pbListHead) * ubUnitSize; 
		*pbListHead = *((CHAR *)pvArray + (*pbListHead) * ubUnitSize + bOffsetNextUnit);
	}
	os_exit_critical();

	return pvUnit;
}

void array_linked_list_put(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bUnitNum, CHAR bOffsetNextUnit)
{
	CHAR bNodeIdx = array_linked_list_get_index(pvUnit, pvArray, ubUnitSize, bUnitNum);
	if (bNodeIdx >= 0)
	{				
		*((CHAR *)pvUnit + bOffsetNextUnit) = *pbListHead;
		*pbListHead = bNodeIdx;		
	}
}

void array_linked_list_put_safe(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bUnitNum, CHAR bOffsetNextUnit)
{
	os_critical_init();

	CHAR bNodeIdx = array_linked_list_get_index(pvUnit, pvArray, ubUnitSize, bUnitNum);
	if (bNodeIdx >= 0)
	{
		os_enter_critical();
		{
			*((CHAR *)pvUnit + bOffsetNextUnit) = *pbListHead;
			*pbListHead = bNodeIdx;
		}
		os_exit_critical();
	}
}

void array_linked_list_put_tail(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bUnitNum, CHAR bOffsetNextUnit)
{
	CHAR bNodeIdx = array_linked_list_get_index(pvUnit, pvArray, ubUnitSize, bUnitNum);
	if (bNodeIdx >= 0)
	{				
		CHAR bNode = *pbListHead; 
		if (bNode >= 0)
		{
			CHAR bNextNode; 
			do {
				bNextNode = *((CHAR *)pvArray + bNode * ubUnitSize + bOffsetNextUnit);
				if (bNextNode < 0) //* 到了尾部
				{
					*((CHAR *)pvArray + bNode * ubUnitSize + bOffsetNextUnit) = bNodeIdx; 						
					break;
				}

				bNode = bNextNode;
			} while (TRUE); 
		}
		else
			*pbListHead = bNodeIdx;
			
		*((CHAR *)pvUnit + bOffsetNextUnit) = -1; 
	}
}

void array_linked_list_put_tail_safe(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bUnitNum, CHAR bOffsetNextUnit)
{
	os_critical_init();

	CHAR bNodeIdx = array_linked_list_get_index(pvUnit, pvArray, ubUnitSize, bUnitNum);
	if (bNodeIdx >= 0)
	{
		os_enter_critical();
		{
			CHAR bNode = *pbListHead;
			if (bNode >= 0)
			{
				CHAR bNextNode;
				do {
					bNextNode = *((CHAR *)pvArray + bNode * ubUnitSize + bOffsetNextUnit);
					if (bNextNode < 0) //* 到了尾部
					{
						*((CHAR *)pvArray + bNode * ubUnitSize + bOffsetNextUnit) = bNodeIdx;
						break;
					}

					bNode = bNextNode;
				} while (TRUE);
			}
			else
				*pbListHead = bNodeIdx;

			*((CHAR *)pvUnit + bOffsetNextUnit) = -1;
		}
		os_exit_critical();
	}
}

void array_linked_list_del(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit)
{
	CHAR bNextNode = *pbListHead;
	CHAR bPrevNode = -1;
	while (bNextNode >= 0)
	{
		//* 找到要摘除的节点并摘除之
		if ((UCHAR *)pvUnit == (UCHAR *)pvArray + bNextNode * ubUnitSize)
		{
			if (bPrevNode >= 0)
				*((CHAR *)pvArray + bPrevNode * ubUnitSize + bOffsetNextUnit) = *((CHAR *)pvUnit + bOffsetNextUnit); 
			else
				*pbListHead = *((CHAR *)pvUnit + bOffsetNextUnit); 

			break;
		}

		bPrevNode = bNextNode;
		bNextNode = *((CHAR *)pvArray + bNextNode * ubUnitSize + bOffsetNextUnit); 
	}
}

void array_linked_list_del_safe(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit)
{
	os_critical_init();

	os_enter_critical();
	{
		CHAR bNextNode = *pbListHead;
		CHAR bPrevNode = -1;
		while (bNextNode >= 0)
		{
			//* 找到要摘除的节点并摘除之
			if ((UCHAR *)pvUnit == (UCHAR *)pvArray + bNextNode * ubUnitSize)
			{
				if (bPrevNode >= 0)
					*((CHAR *)pvArray + bPrevNode * ubUnitSize + bOffsetNextUnit) = *((CHAR *)pvUnit + bOffsetNextUnit);
				else
					*pbListHead = *((CHAR *)pvUnit + bOffsetNextUnit);

				break;
			}

			bPrevNode = bNextNode;
			bNextNode = *((CHAR *)pvArray + bNextNode * ubUnitSize + bOffsetNextUnit);
		}
	}
	os_exit_critical();
}

void *array_linked_list_next(CHAR *pbNextUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit, CHAR *pbUnitIdx)
{
	if (*pbListHead >= 0)
	{
		CHAR bUnit;
		if (*pbNextUnit >= 0)
			bUnit = *pbNextUnit;
		else
			bUnit = *pbListHead;

		if (pbUnitIdx)
			*pbUnitIdx = bUnit; 

		*pbNextUnit = *((CHAR *)pvArray + bUnit * ubUnitSize + bOffsetNextUnit);
		return (CHAR *)pvArray + bUnit * ubUnitSize;
	}
	else
		return NULL; 
}

void *array_linked_list_next_ext(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit)
{
	if (pvUnit)
	{
		CHAR bNextUnit = *pbListHead;
		void *pvNextUnit;
		while (bNextUnit >= 0)
		{
			pvNextUnit = (UCHAR *)pvArray + bNextUnit * ubUnitSize;
			if (pvUnit == (void *)pvNextUnit) //* 找到当前节点
			{
				//* 取下一个节点
				bNextUnit = *((CHAR *)pvArray + bNextUnit * ubUnitSize + bOffsetNextUnit);
				if (bNextUnit >= 0)
					return (CHAR *)pvArray + bNextUnit * ubUnitSize;
				break;
			}

			bNextUnit = *((CHAR *)pvArray + bNextUnit * ubUnitSize + bOffsetNextUnit);
		}
	}
	else
	{
		if(*pbListHead >= 0)
			return (CHAR *)pvArray + (*pbListHead) * ubUnitSize; 
	}

	return NULL; 
}

CHAR *strtok_safe(CHAR **ppszStart, const CHAR *pszSplitStr)
{
    if (NULL == *ppszStart)
        return NULL;

    CHAR *pszItem = NULL;

    CHAR *pszStart = *ppszStart;
    INT i = 0;
    CHAR ch;
    while ((ch = pszStart[i]) != 0x00)
    {
        CHAR bSplit;
        BOOL blIsNotFound = TRUE;
        INT k = 0;
        while ((bSplit = pszSplitStr[k++]) != 0x00)
        {
            if (bSplit == ch)
            {
                pszStart[i] = 0;
                blIsNotFound = FALSE;
                break;
            }
        }

        if (blIsNotFound)
        {
            if (NULL == pszItem)
                pszItem = pszStart + i;
        }
        else
        {
            if (pszItem)
            {
                *ppszStart = pszStart + i + 1;
                return pszItem;
            }
        }

        i++;
    }

    *ppszStart = NULL;
    return pszItem;
}

UINT rand_big(void)
{
	UINT unRandomVal; 
	((UCHAR *)&unRandomVal)[0] = (UCHAR)(rand() % 256); 
	((UCHAR *)&unRandomVal)[1] = (UCHAR)(rand() % 256);
	((UCHAR *)&unRandomVal)[2] = (UCHAR)(rand() % 256);
	((UCHAR *)&unRandomVal)[3] = (UCHAR)(rand() % 256);

	return unRandomVal; 
}

UCHAR *rand_any_bytes(UCHAR *pubRandSeq, UINT unSeqLen)
{
	UINT i; 
	for (i = 0; i < unSeqLen; i++)	
		pubRandSeq[i] = (UCHAR)(rand() % 256); 

	return pubRandSeq; 
}

const CHAR *hex_to_str_8(UCHAR ubVal, CHAR szDst[3], BOOL blIsUppercase)
{
	szDst[0] = ubVal >> 4;
	szDst[1] = ubVal & 0x0F;	
	
	hex_to_char(szDst[0], blIsUppercase); 
	hex_to_char(szDst[1], blIsUppercase); 
	szDst[2] = 0;

	return szDst;
}

const CHAR *hex_to_str_no_lz_8(UCHAR ubVal, CHAR szDst[3], BOOL blIsUppercase, CHAR *pbBytes)
{
	if (!ubVal)
	{
		szDst[0] = '0';
		szDst[1] = 0;

		if (pbBytes)
			*pbBytes = 1; 

		return szDst;
	}

	szDst[0] = ubVal >> 4;
	szDst[1] = ubVal & 0x0F;

	CHAR bBytes = 2; 
	if (szDst[0])
	{
		hex_to_char(szDst[0], blIsUppercase); 
		hex_to_char(szDst[1], blIsUppercase);
		szDst[bBytes] = 0;
	}
	else
	{
		hex_to_char_no_lz(szDst[0], szDst[1], blIsUppercase); 
		bBytes = 1;
		szDst[bBytes] = 0;		
	}

	if (pbBytes)
		*pbBytes = bBytes;

	return szDst;
}

const CHAR *hex_to_str_16(USHORT usVal, CHAR szDst[5], BOOL blIsUppercase, BOOL blIsBigEndian)
{
	if (blIsBigEndian)
	{
		szDst[0] = (((UCHAR*)&usVal)[0]) >> 4;
		szDst[1] = (((UCHAR*)&usVal)[0]) & 0x0F;
		szDst[2] = (((UCHAR*)&usVal)[1]) >> 4;
		szDst[3] = (((UCHAR*)&usVal)[1]) & 0x0F;
	}
	else
	{
		szDst[0] = (((UCHAR*)&usVal)[1]) >> 4;
		szDst[1] = (((UCHAR*)&usVal)[1]) & 0x0F;
		szDst[2] = (((UCHAR*)&usVal)[0]) >> 4;
		szDst[3] = (((UCHAR*)&usVal)[0]) & 0x0F;
	}	

	hex_to_char(szDst[0], blIsUppercase);
	hex_to_char(szDst[1], blIsUppercase);
	hex_to_char(szDst[2], blIsUppercase);
	hex_to_char(szDst[3], blIsUppercase);
	szDst[4] = 0;
	return szDst;
}

const CHAR *hex_to_str_no_lz_16(USHORT usVal, CHAR szDst[5], BOOL blIsUppercase, BOOL blIsBigEndian, CHAR *pbBytes)
{
	if (!usVal)
	{
		szDst[0] = '0';
		szDst[1] = 0;

		if (pbBytes)
			*pbBytes = 1;

		return szDst;
	}

	if (blIsBigEndian)
	{
		szDst[0] = (((UCHAR*)&usVal)[0]) >> 4;
		szDst[1] = (((UCHAR*)&usVal)[0]) & 0x0F;
		szDst[2] = (((UCHAR*)&usVal)[1]) >> 4;
		szDst[3] = (((UCHAR*)&usVal)[1]) & 0x0F;
	}
	else
	{
		szDst[0] = (((UCHAR*)&usVal)[1]) >> 4;
		szDst[1] = (((UCHAR*)&usVal)[1]) & 0x0F;
		szDst[2] = (((UCHAR*)&usVal)[0]) >> 4;
		szDst[3] = (((UCHAR*)&usVal)[0]) & 0x0F;
	}

	//* 计算前导零个数
	CHAR i = 0, k = 0;
	for (; i < 3; i++)
	{
		if (szDst[i])
			break;

		k++;
	}

	if (pbBytes)
		*pbBytes = 4 - k; 

	hex_to_char(szDst[0], blIsUppercase);
	hex_to_char(szDst[1], blIsUppercase);
	hex_to_char(szDst[2], blIsUppercase);
	hex_to_char(szDst[3], blIsUppercase);

	//* k为0，没有前导0
	if (!k)
	{
		szDst[4] = 0;		
	}
	else 
	{
		//* 去掉前导0
		CHAR bMovNum = 4 - k;
		for (i = 0; i < bMovNum; i++)
			szDst[i] = szDst[k++];
		szDst[bMovNum] = 0;
	}	

	return szDst;
}

CHAR ascii_to_hex_4(CHAR ch)
{
	if (ch >= '0' && ch <= '9')
		return (ch - '0');
	if (ch >= 'A' && ch <= 'F')
		return (ch - 'A') + 10;
	if (ch >= 'a' && ch <= 'f')
		return (ch - 'a') + 10; 

	return 0;
}

USHORT ascii_to_hex_16(const CHAR *pszAscii)
{
	USHORT usValue = 0;
	INT i; 

	for (i = 0; i < strlen(pszAscii); i++)	
		usValue = (usValue << 4) + ascii_to_hex_4(pszAscii[i]);

	return usValue;
}

in_addr_t inet_addr_small(const char *pszIP)
{    
    in_addr_t unAddr;
    CHAR *pszStart = (CHAR *)pszIP, *pszDot;
    UINT unLen = (UINT)strlen(pszIP);
    pszDot = mem_char(pszStart, '.', unLen);
    ((UCHAR *)&unAddr)[3] = (UCHAR)atoi(pszStart);
    pszStart = pszDot + 1;
    pszDot = mem_char(pszStart, '.', unLen - (pszStart - pszIP));
    ((UCHAR *)&unAddr)[2] = (UCHAR)atoi(pszStart);
    pszStart = pszDot + 1;
    pszDot = mem_char(pszStart, '.', unLen - (pszStart - pszIP));
    ((UCHAR *)&unAddr)[1] = (UCHAR)atoi(pszStart);
    pszStart = pszDot + 1;
    ((UCHAR *)&unAddr)[0] = (UCHAR)atoi(pszStart);

    return unAddr;
}

in_addr_t inet_addr(const char *pszIP)
{
    in_addr_t unAddr;
    CHAR *pszStart = (CHAR *)pszIP, *pszDot;
    UINT unLen = (UINT)strlen(pszIP);
    pszDot = mem_char(pszStart, '.', unLen);
    ((UCHAR *)&unAddr)[0] = (UCHAR)atoi(pszStart);
    pszStart = pszDot + 1;
    pszDot = mem_char(pszStart, '.', unLen - (pszStart - pszIP));
    ((UCHAR *)&unAddr)[1] = (UCHAR)atoi(pszStart);
    pszStart = pszDot + 1;
    pszDot = mem_char(pszStart, '.', unLen - (pszStart - pszIP));
    ((UCHAR *)&unAddr)[2] = (UCHAR)atoi(pszStart);
    pszStart = pszDot + 1;
    ((UCHAR *)&unAddr)[3] = (UCHAR)atoi(pszStart);

    return unAddr;
}

char *inet_ntoa(struct in_addr stInAddr)
{
    static char szAddr[20]; 
    UCHAR *pubAddr = (UCHAR *)&stInAddr.s_addr;
    sprintf(szAddr, "%d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    return szAddr; 
}

char *inet_ntoa_ext(in_addr_t unAddr)
{
    static char szAddr[20];
    UCHAR *pubAddr = (UCHAR *)&unAddr; 
    sprintf(szAddr, "%d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    return szAddr;
}

char *inet_ntoa_safe(struct in_addr stInAddr, char *pszAddr)
{
    UCHAR *pubAddr = (UCHAR *)&stInAddr.s_addr;
    sprintf(pszAddr, "%d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    return pszAddr;
}

char *inet_ntoa_safe_ext(in_addr_t unAddr, char *pszAddr)
{
    UCHAR *pubAddr = (UCHAR *)&unAddr;
    sprintf(pszAddr, "%d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    return pszAddr;
}

const CHAR *get_ip_proto_name(UCHAR ubProto)
{
    switch ((EN_IPPROTO)ubProto)
    {
    case IPPROTO_ICMP:
        return "icmp"; 

    case IPPROTO_IGMP:
        return "igmp"; 

    case IPPROTO_TCP:
        return "tcp"; 

    case IPPROTO_UDP: 
        return "udp"; 

	case IPPROTO_ICMPv6:
		return "icmpv6"; 

    case IPPROTO_RAW:
        return "raw ip"; 

    default:
        return "unsupported";
    }
}

//* 几级域名，也就是域名分了几段，为封装dns查询报文提供数据支持
INT get_level_of_domain_name(const CHAR *pszDomainName, INT *pnBytesOf1stSeg)
{
    INT i, nCount = 1, nBytesOf1stSeg = 0;
    INT nDomainNameLen = strlen(pszDomainName);
    for (i = 0; i < nDomainNameLen; i++)
    {
        if (pszDomainName[i] == '.')
        {
            nCount++;
            if (0 == nBytesOf1stSeg)
                nBytesOf1stSeg = i;
        }
    }

    if (0 == nBytesOf1stSeg)
        nBytesOf1stSeg = i;
    *pnBytesOf1stSeg = nBytesOf1stSeg;

    return nCount;
}

#if SUPPORT_ETHERNET
//* 判断mac地址是否匹配
BOOL ethernet_mac_matched(const UCHAR *pubaMacAddr1, const UCHAR *pubaMacAddr2)
{
    INT i; 
    for (i = 0; i < ETH_MAC_ADDR_LEN; i++)
    {
        if (pubaMacAddr1[i] != pubaMacAddr2[i])
            return FALSE; 
    }

    return TRUE; 
}

BOOL is_mac_broadcast_addr(const UCHAR *pubaMacAddr)
{
	if (pubaMacAddr[0] == 0x33 && pubaMacAddr[1] == 0x33)
		return TRUE; 

    INT i; 
    for (i = 0; i < ETH_MAC_ADDR_LEN; i++)
    {
        if (pubaMacAddr[i] != 0xFF)
            return FALSE;
    }

    return TRUE;
}
#endif

INT bit8_matched_from_left(UCHAR ch1, UCHAR ch2, UCHAR ubCmpBits)
{
	UCHAR i;
	UCHAR ubMask = 0x80;
	for (i = 0; i < ubCmpBits; i++)
	{
		if ((ch1 & ubMask) != (ch2 & ubMask))
			break;
		ubMask >>= 1;
	}

	return i;
}

//* 利用冯.诺伊曼算法生成近似均匀分布的哈希值，其输出并不在16位结果值生成后就立即结束，而是64位数据耗尽后结束，其生成结果最大为32位值：
//* 1. 从LSB开始，每次选择两位；
//* 2. 如果两位为00或11，则丢弃；
//* 3. 如果两位为01，则输出0；
//* 4. 如果两位为10，则输出1；
//* 5. 回到1，开始下一组，直至64位结束。
UINT hash_von_neumann(ULONGLONG ullKey)
{
	UINT unResult = 0;
	UCHAR i, ubBits = 0;

	for (i = 0; i < sizeof(ullKey) * 8; i += 2)
	{
		UCHAR ubVal = ((UCHAR)ullKey) & 0x03;
		if (1 == ubVal || 2 == ubVal)
		{
			if (ubVal == 2)
				unResult |= 1 << ubBits;
			ubBits++;
		}

		ullKey >>= 2;
	}

	return unResult;
}

#if SUPPORT_IPV6
const CHAR *inet6_ntoa(const UCHAR ubaIpv6[16], CHAR szIpv6[40])
{
	USHORT *pusIpv6 = (USHORT *)ubaIpv6;
	CHAR i, k;
	CHAR bBytes;
	CHAR bStartIdx, bZeroFieldIdx = -1;
	CHAR bCount = 0, bZeroCount = 0;

	//* 查找需要压缩展示的全0字段：
	//* 1) 单0字段不能压缩；
	//* 2) 如果存在多组连续全零字段，只压缩全零字段最长的那组
	//* 3) 如果存在多个相等长度的全零字段，只压缩第一组连续全零字段
	//* 注意：一个Ipv6地址只能按照上述规则压缩一组连续全零字段
	for (i = 0; i < 8; i++)
	{
		if (!pusIpv6[i])
		{
			if (!bCount)
				bStartIdx = i;
			bCount++;
		}
		else
		{
			if (bCount > bZeroCount)
			{
				bZeroFieldIdx = bStartIdx;
				bZeroCount = bCount;
			}

			bCount = 0;
		}
	}

	if (bCount > bZeroCount)
	{
		bZeroFieldIdx = bStartIdx;
		bZeroCount = bCount;
	}

	k = 0;
	for (i = 0; i < 8; i++)
	{
		if (i != bZeroFieldIdx)
		{
			if (i)
				szIpv6[k++] = ':';
			hex_to_str_no_lz_16(pusIpv6[i], &szIpv6[k], FALSE, TRUE, &bBytes);
			k += bBytes;
		}
		else
		{
			szIpv6[k++] = ':';
			i += bZeroCount - 1;
			if (i == 7)
			{
				szIpv6[k++] = ':';
				break; 
			}
		}
	}

	szIpv6[k] = 0;

	return szIpv6;
}

const UCHAR *inet6_aton(const CHAR *pszIpv6, UCHAR ubaIpv6[16])
{
	CHAR bZeroFieldIdx = -1, bCount = 0;
	CHAR i;

	//* 找出压缩的全零字段
	BOOL blIsExecptNextColon = FALSE;
	CHAR bIpv6Len = (CHAR)strlen(pszIpv6);
	for (i = 0; i < bIpv6Len; i++)
	{
		if (pszIpv6[i] == ':')
		{
			if (i && i < bIpv6Len - 1)
				bCount++;
			if (blIsExecptNextColon && bZeroFieldIdx < 0)
				bZeroFieldIdx = i;
			else
				blIsExecptNextColon = TRUE;
		}
		else
			blIsExecptNextColon = FALSE;
	}

	//* 存在压缩字段，则计算压缩字段的数量
	if (bZeroFieldIdx > 0)
		bCount = 8 - bCount;
	else
		bCount = 0;

	//* 解析字符串将其转成16进制的Ipv6地址
	CHAR szIpv6[40];
	memcpy(szIpv6, pszIpv6, bIpv6Len); //* 复制源字符串，以确保调用stotok_safe()函数时不改变入口参数pszIpv6的值
	szIpv6[bIpv6Len] = 0;
	CHAR *pszStart = szIpv6;
	CHAR *pszField;
	USHORT usVal;
	i = 0;
	do {
		if (NULL != (pszField = strtok_safe(&pszStart, ":")))
		{
			if (bZeroFieldIdx > 0 && (CHAR *)&szIpv6[bZeroFieldIdx + 1] == pszField)
			{
				CHAR bFilledZeroNum = bCount * 2;
				memset(&ubaIpv6[i], 0, bFilledZeroNum);
				i += bFilledZeroNum;
			}

			usVal = ascii_to_hex_16(pszField);
			ubaIpv6[i] = ((UCHAR *)&usVal)[1];
			ubaIpv6[i + 1] = ((UCHAR *)&usVal)[0];
			i += 2;
		}
		else
			break;
	} while (TRUE);

	if (bZeroFieldIdx > 0 && bZeroFieldIdx + 1 == bIpv6Len)
		memset(&ubaIpv6[i], 0, bCount * 2);

	return ubaIpv6;
}

INT ipv6_addr_cmp(const UCHAR *pubAddr1, const UCHAR *pubAddr2, UCHAR ubBitsToCompare)
{
	UCHAR ubBytes = ubBitsToCompare / 8;
	INT nRtnVal = memcmp(pubAddr1, pubAddr2, (size_t)ubBytes);
	if (0 == nRtnVal)
	{
		UCHAR ubBitNum = ubBitsToCompare % 8;
		if (ubBitNum)
		{
			UCHAR ubMask = 0xFF << (8 - ubBitNum);
			UCHAR ubAddr1 = pubAddr1[ubBytes] & ubMask;
			UCHAR ubAddr2 = pubAddr2[ubBytes] & ubMask;
			if (ubAddr1 == ubAddr2)
				return 0;
			else if (ubAddr1 > ubAddr2)
				return 1;
			else
				return -1;
		}
		else
			return 0;
	}

	return nRtnVal;
}

INT ipv6_prefix_matched_bits(const UCHAR ubaAddr1[16], const UCHAR ubaAddr2[16], UCHAR ubPrefixBitsLen)
{
	UCHAR ubCmpBytes = ubPrefixBitsLen / 8;
	UCHAR ubCmpBits = ubPrefixBitsLen % 8;
	UCHAR i, bMatchedBytes = 0;

	for (i = 0; i < ubCmpBytes; i++)
	{
		if (ubaAddr1[i] != ubaAddr2[i])
			goto __lblMacthedBits;
		bMatchedBytes++;
	}

	if (ubCmpBits)
	{
		return ubPrefixBitsLen - ubCmpBits + bit8_matched_from_left(ubaAddr1[i], ubaAddr2[i], ubCmpBits);
	}
	else
		return ubPrefixBitsLen;

__lblMacthedBits:
	return bMatchedBytes * 8 + bit8_matched_from_left(ubaAddr1[i], ubaAddr2[i], 8);
}
#endif //* #if SUPPORT_IPV6


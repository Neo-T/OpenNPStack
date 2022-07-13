#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "netif/netif.h"
#include "ip/ip_frame.h"

#define SYMBOL_GLOBALS
#include "onps_utils.h"
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
    if (*ppstSLList)
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
        if (pstNextNode->uniData.pvAddr == pvData)
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

in_addr_t inet_addr(const char *pszIP)
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

in_addr_t inet_addr_small(const char *pszIP)
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

char *inet_ntoa_safe(struct in_addr stInAddr, char *pszAddr)
{
    UCHAR *pubAddr = (UCHAR *)&stInAddr.s_addr;
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

    case IPPROTO_RAW:
        return "raw ip"; 

    default:
        return "unsupported";
    }
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
    INT i; 
    for (i = 0; i < ETH_MAC_ADDR_LEN; i++)
    {
        if (pubaMacAddr[i] != 0xFF)
            return FALSE;
    }

    return TRUE;
}
#endif


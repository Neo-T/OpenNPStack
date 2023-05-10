/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "ip/icmp.h"

#if SUPPORT_IPV6
#include "netif/netif.h"
#include "ip/icmpv6.h"
#endif

#define SYMBOL_GLOBALS
#include "net_tools/ping.h"
#undef SYMBOL_GLOBALS

#if NETTOOLS_PING
INT ping_start(INT family, EN_ONPSERR *penErr)
{    	
	return onps_input_new(family, (AF_INET == family) ? IPPROTO_ICMP : IPPROTO_ICMPv6, penErr); 
}

void ping_end(INT nPing)
{
    onps_input_free(nPing);
}

INT ping_send(INT family, INT nPing, const void *pvDstAddr, USHORT usSeqNum, UCHAR ubTTL, const UCHAR *pubEcho, UCHAR ubEchoLen, EN_ONPSERR *penErr)
{
	if (AF_INET == family)
	{
		in_addr_t unDstAddr = inet_addr(pvDstAddr); 
		return icmp_send_echo_reqest(nPing, (USHORT)(nPing + 1), usSeqNum, ubTTL, unDstAddr, pubEcho, (UINT)ubEchoLen, penErr);
	}
	else
	{
		UCHAR ubaDstAddr[16];
		return icmpv6_send_echo_request(nPing, (UCHAR *)inet6_aton(pvDstAddr, ubaDstAddr), (USHORT)(nPing + 1), usSeqNum, pubEcho, ubEchoLen, penErr);
	}
}

INT ping_recv(INT nPing, in_addr_t *punFromAddr, USHORT *pusSeqNum, UCHAR *pubDataBuf, UCHAR ubDataBufSize, UCHAR *pubTTL, UCHAR ubWaitSecs, EN_ONPSERR *penErr)
{
    UCHAR *pubPacket; 

	EN_ONPSERR enErr = ERRNO; 
	UCHAR ubType, ubCode; 
    INT nRcvedBytes = onps_input_recv_icmp(nPing, &pubPacket, punFromAddr, pubTTL, &ubType, &ubCode, ubWaitSecs, &enErr);
    if (nRcvedBytes > 0)
    {
        PST_ICMP_ECHO_HDR pstEchoHdr = (PST_ICMP_ECHO_HDR)(pubPacket + sizeof(ST_ICMP_HDR));        

        //* 应答报文携带的identifier是否匹配，如果不匹配则直接返回错误，没必要继续等待，因为onps input模块收到的一定是当前ping链路到达的报文
        if (nPing + 1 == (INT)htons(pstEchoHdr->usIdentifier))
        {
            //* 复制echo数据到用户接收缓冲区
            INT nCpyBytes = (INT)ubDataBufSize > nRcvedBytes - sizeof(ST_ICMP_HDR) - sizeof(ST_ICMP_ECHO_HDR) ? nRcvedBytes - sizeof(ST_ICMP_HDR) - sizeof(ST_ICMP_ECHO_HDR) : (INT)ubDataBufSize;
            memcpy(pubDataBuf, pubPacket + sizeof(ST_ICMP_HDR) + sizeof(ST_ICMP_ECHO_HDR), nCpyBytes);

            if (pusSeqNum)
                *pusSeqNum = htons(pstEchoHdr->usSeqNum);

            return nCpyBytes;
        }
        else
        {
            //* 不匹配，则直接返回0，显式地告诉用户未收到应答
            return 0;
        }
    }
	else if(nRcvedBytes < 0)
	{		
		if (ERRNO != enErr)
		{
			if (penErr)
				*penErr = enErr; 
		}
		else
		{
	#if SUPPORT_PRINTF
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif			
			printf("%s\r\n", icmp_get_description(ubType, ubCode));			
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif			
			nRcvedBytes = 0; 
		}
	}
	else; 
    
    return nRcvedBytes; 
}

//* 实现ping操作
INT ping(INT nPing, const CHAR *pszDstAddr, USHORT usSeqNum, UCHAR ubTTL, UINT(*pfunGetCurMSecs)(void), 
	void(*pfunRcvHandler)(USHORT usIdentifier, void *pvFromAddr, USHORT usSeqNum, UCHAR *pubEchoData, UCHAR ubEchoDataLen, UCHAR ubTTL, UCHAR ubElapsedMSecs), UCHAR ubWaitSecs, EN_ONPSERR *penErr)
{
	CHAR bFamily;
	if (!onps_input_get((INT)nPing, IOPT_GET_ICMPAF, &bFamily, penErr))
		return -1;

    UINT unStartMillisecs = pfunGetCurMSecs(); 
    INT nRtnVal = ping_send(bFamily, nPing, pszDstAddr, usSeqNum, ubTTL, "I am Trinity, Neo. Welcome to zion.\x00", strlen("I am Trinity, Neo. Welcome to zion.\x00") + 1, penErr);
    if (nRtnVal < 0)
        return -1; 

	INT nRcvedBytes; 
	if (AF_INET == bFamily) 
	{
		in_addr_t unFromAddr;
		USHORT usReplySeqNum;
		UCHAR ubReplyTTL;
		UCHAR ubaEchoData[48];
		nRcvedBytes = ping_recv(nPing, &unFromAddr, &usReplySeqNum, ubaEchoData, sizeof(ubaEchoData) - 1, &ubReplyTTL, ubWaitSecs, penErr);
		if (nRcvedBytes > 0)
		{
			ubaEchoData[nRcvedBytes] = 0;
			pfunRcvHandler(nPing + 1, &unFromAddr, usReplySeqNum, ubaEchoData, (UCHAR)nRcvedBytes, ubReplyTTL, (UCHAR)(pfunGetCurMSecs() - unStartMillisecs));
		}
	}
	else
	{
		UCHAR ubaFromAddr[16];
		USHORT usReplySeqNum;		
		UCHAR ubaEchoData[48];
		nRcvedBytes = ping_recv(nPing, (in_addr_t *)ubaFromAddr, &usReplySeqNum, ubaEchoData, sizeof(ubaEchoData) - 1, NULL, ubWaitSecs, penErr);
		if (nRcvedBytes > 0)
		{
			ubaEchoData[nRcvedBytes] = 0;
			pfunRcvHandler(nPing + 1, ubaFromAddr, usReplySeqNum, ubaEchoData, (UCHAR)nRcvedBytes, 0, (UCHAR)(pfunGetCurMSecs() - unStartMillisecs));
		}
	}

    return nRcvedBytes; 
}
#endif

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
#if SUPPORT_IPV6
	return onps_input_new(family, (AF_INET == family) ? IPPROTO_ICMP : IPPROTO_ICMPv6, penErr); 
#else
	return onps_input_new((AF_INET == family) ? IPPROTO_ICMP : IPPROTO_ICMPv6, penErr);
#endif
}

void ping_end(INT nPing)
{
    onps_input_free(nPing);
}

INT ping_send(INT family, INT nPing, const void *pvDstAddr, USHORT usSeqNum, UCHAR ubTTL, const UCHAR *pubEcho, UCHAR ubEchoLen, EN_ONPSERR *penErr)
{
#if SUPPORT_IPV6
	if (AF_INET == family)
	{
#endif
		in_addr_t unDstAddr = inet_addr(pvDstAddr); 
		return icmp_send_echo_reqest(nPing, (USHORT)(nPing + 1), usSeqNum, ubTTL, unDstAddr, pubEcho, (UINT)ubEchoLen, penErr);

#if SUPPORT_IPV6
	}
	else
	{
		UCHAR ubaDstAddr[16];
		return icmpv6_send_echo_request(nPing, (UCHAR *)inet6_aton(pvDstAddr, ubaDstAddr), (USHORT)(nPing + 1), usSeqNum, pubEcho, ubEchoLen, penErr);
	}
#endif
}

INT ping_recv(INT nPing, void *pvFromAddr, USHORT *pusSeqNum, UCHAR *pubDataBuf, UCHAR ubDataBufSize, UCHAR *pubTTL, UCHAR *pubType, UCHAR *pubCode, UCHAR ubWaitSecs, EN_ONPSERR *penErr)
{
    UCHAR *pubPacket; 
 	
    INT nRcvedBytes = onps_input_recv_icmp(nPing, &pubPacket, pvFromAddr, pubTTL, pubType, pubCode, ubWaitSecs, penErr);
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
    
    return nRcvedBytes; 
}

//* 实现ping操作
INT ping(INT nPing, const CHAR *pszDstAddr, USHORT usSeqNum, UCHAR ubTTL, UINT(*pfunGetCurMSecs)(void), PFUN_PINGRCVHANDLER pfunRcvHandler, 
			PFUN_PINGERRHANDLER pfunErrHandler, UCHAR ubWaitSecs, void *pvParam, EN_ONPSERR *penErr)
{
	CHAR bFamily = AF_INET;
#if SUPPORT_IPV6
	if (!onps_input_get((INT)nPing, IOPT_GETICMPAF, &bFamily, penErr))
		return -1;
#endif

    UINT unStartMillisecs = pfunRcvHandler ? pfunGetCurMSecs() : 0;
    INT nRtnVal = ping_send(bFamily, nPing, pszDstAddr, usSeqNum, ubTTL, "I am Trinity, Neo. Welcome to zion.\x00", strlen("I am Trinity, Neo. Welcome to zion.\x00") + 1, penErr);
    if (nRtnVal < 0)
        return -1; 

	EN_ONPSERR enErr = ERRNO;
	UCHAR ubType, ubCode; 
	INT nRcvedBytes; 
	if (AF_INET == bFamily) 
	{
		in_addr_t unFromAddr;
		USHORT usReplySeqNum;
		UCHAR ubReplyTTL;
		UCHAR ubaEchoData[48];
		nRcvedBytes = ping_recv(nPing, &unFromAddr, &usReplySeqNum, ubaEchoData, sizeof(ubaEchoData) - 1, &ubReplyTTL, &ubType, &ubCode, ubWaitSecs, &enErr);
		if (nRcvedBytes > 0)
		{
			ubaEchoData[nRcvedBytes] = 0;
			pfunRcvHandler(nPing + 1, &unFromAddr, usReplySeqNum, ubaEchoData, (UCHAR)nRcvedBytes, ubReplyTTL, (UCHAR)(pfunRcvHandler ? pfunGetCurMSecs() - unStartMillisecs : 0), pvParam);
		}
	}
#if SUPPORT_IPV6
	else
	{
		UCHAR ubaFromAddr[16];
		USHORT usReplySeqNum;		
		UCHAR ubaEchoData[48];
		nRcvedBytes = ping_recv(nPing, ubaFromAddr, &usReplySeqNum, ubaEchoData, sizeof(ubaEchoData) - 1, NULL, &ubType, &ubCode, ubWaitSecs, &enErr);
		if (nRcvedBytes > 0)
		{
			ubaEchoData[nRcvedBytes] = 0;
			pfunRcvHandler(nPing + 1, ubaFromAddr, usReplySeqNum, ubaEchoData, (UCHAR)nRcvedBytes, 0, (UCHAR)(pfunRcvHandler ? pfunGetCurMSecs() - unStartMillisecs : 0), pvParam); 
		}
	}
#endif

	if (nRcvedBytes < 0)
	{
		if (ERRNO != enErr)
		{
			if (penErr)
				*penErr = enErr; 
		}
		else
		{
			if (pfunErrHandler)
				pfunErrHandler(nPing + 1, pszDstAddr, ubType, ubCode, pvParam);
			else
			{
		#if SUPPORT_PRINTF
			#if PRINTF_THREAD_MUTEX
				os_thread_mutex_lock(o_hMtxPrintf);
			#endif	
			#if SUPPORT_IPV6
				printf("ping %s failed, %s\r\n", pszDstAddr, (AF_INET == bFamily) ? icmp_get_description(ubType, ubCode) : icmpv6_get_description(ubType, ubCode));
			#else
				printf("ping %s failed, %s\r\n", pszDstAddr, icmp_get_description(ubType, ubCode));
			#endif
			#if PRINTF_THREAD_MUTEX
				os_thread_mutex_unlock(o_hMtxPrintf);
			#endif
		#endif							
			}

			nRcvedBytes = 0;
		}
	}

    return nRcvedBytes; 
}

#if NETTOOLS_TELNETSRV && NVTCMD_PING_EN
#include "net_tools/net_virtual_terminal.h"
#include "telnet/os_nvt.h"
#include "telnet/nvt_cmd.h"

//* ping控制块，包含ping启动参数机统计结果
typedef struct _STCB_PING_ {
    ST_PING_STARTARGS stArgs; 
    struct {
        UINT unRttTotal;
        UINT unRttMin;
        UINT unRttMax;
    } stStatistics; 
} STCB_PING, *PSTCB_PING;

static void ping_recv_handler(USHORT usIdentifier, void *pvFromAddr, USHORT usSeqNum, UCHAR *pubEchoData, UCHAR ubEchoDataLen, UCHAR ubTTL, UCHAR ubElapsedMSecs, void *pvParam)
{
    PSTCB_PING pstcbPing = (PSTCB_PING)pvParam; 
    CHAR szSrcIp[40];   

    pstcbPing->stStatistics.unRttTotal += ubElapsedMSecs; 
    if (ubElapsedMSecs > pstcbPing->stStatistics.unRttMax)
        pstcbPing->stStatistics.unRttMax = ubElapsedMSecs; 
    if (ubElapsedMSecs < pstcbPing->stStatistics.unRttMin)
        pstcbPing->stStatistics.unRttMin = ubElapsedMSecs; 

#if SUPPORT_IPV6
    nvt_outputf(pstcbPing->stArgs.ullNvtHandle, 512, "<Fr>%s, recv %d bytes, ID=%d, Sequence=%d, Data='%s', TTL=%d, time=%dms\r\n",
                    (AF_INET == pstcbPing->stArgs.nFamily) ? inet_ntoa_safe_ext(*(in_addr_t *)pvFromAddr, szSrcIp) : inet6_ntoa(pvFromAddr, szSrcIp),
                    (UINT)ubEchoDataLen, usIdentifier, usSeqNum, pubEchoData, (UINT)ubTTL, (UINT)ubElapsedMSecs); 
#else
    nvt_outputf(pstcbPing->stArgs.ullNvtHandle, 512, "<Fr>%s, recv %d bytes, ID=%d, Sequence=%d, Data='%s', TTL=%d, time=%dms\r\n",
                    inet_ntoa_safe_ext(*(in_addr_t *)pvFromAddr, szSrcIp), (UINT)ubEchoDataLen, usIdentifier, usSeqNum, pubEchoData, (UINT)ubTTL, (UINT)ubElapsedMSecs);
#endif
}

static void ping_error_handler(USHORT usIdentifier, const CHAR *pszDstAddr, UCHAR ubIcmpErrType, UCHAR ubIcmpErrCode, void *pvParam)
{
    PSTCB_PING pstcbPing = (PSTCB_PING)pvParam; 
#if SUPPORT_IPV6
    nvt_outputf(pstcbPing->stArgs.ullNvtHandle, 256, "ping %s failed, %s\r\n", pszDstAddr, (AF_INET == pstcbPing->stArgs.nFamily) ? icmp_get_description(ubIcmpErrType, ubIcmpErrCode) : icmpv6_get_description(ubIcmpErrType, ubIcmpErrCode));
#else
    nvt_outputf(pstcbPing->stArgs.ullNvtHandle, 256, "ping %s failed, %s\r\n", pszDstAddr, icmp_get_description(ubIcmpErrType, ubIcmpErrCode));
#endif
}

void nvt_cmd_ping_entry(void *pvParam)
{
    UCHAR ubRcvBuf[4];
    INT nRcvBytes, nRtnVal;    
    UINT unSeqNum = 0, unErrCount = 0, unRecvedPackets;

    //* 必须要复制，因为这个函数被设计为单独一个线程/任务
    STCB_PING stcbPing; 
    stcbPing.stArgs = *((PST_PING_STARTARGS)pvParam);
    ((PST_PING_STARTARGS)pvParam)->bIsCpyEnd = TRUE; 

    //* 启动ping     
    EN_ONPSERR enErr;     
    INT nPing; 
#if SUPPORT_IPV6
    if (AF_INET == stcbPing.stArgs.nFamily)
        nPing = ping_start(AF_INET, &enErr); 
    else    
        nPing = ping_start(AF_INET6, &enErr); 
#else
    nPing = ping_start(AF_INET, &enErr); 
#endif
    if (nPing < 0)
    {
        nvt_outputf(stcbPing.stArgs.ullNvtHandle, 256, "ping_start() failed. %s\r\n", onps_error(enErr));
        goto __lblEnd; 
    }

    nvt_output(stcbPing.stArgs.ullNvtHandle, "You can enter \033[01;37mctrl+c\033[0m to abort the ping.\r\n", sizeof("You can enter \033[01;37mctrl+c\033[0m to abort the ping.\r\n") - 1);

    //* 连续ping
    stcbPing.stStatistics.unRttMin = 0xFFFFFFFF; 
    stcbPing.stStatistics.unRttTotal = stcbPing.stStatistics.unRttMax = 0; 
    while (nvt_cmd_exec_enable(stcbPing.stArgs.ullNvtHandle))
    {
        nRcvBytes = nvt_input(stcbPing.stArgs.ullNvtHandle, ubRcvBuf, sizeof(ubRcvBuf));
        if (nRcvBytes)
        {
            if (nRcvBytes == 1 && ubRcvBuf[0] == '\x03')
            {
                nvt_output(stcbPing.stArgs.ullNvtHandle, "\r\n", 2);
                break;
            }
        }
        
        nRtnVal = ping(nPing, stcbPing.stArgs.szDstIp, (USHORT)unSeqNum++, 64, os_get_elapsed_millisecs, ping_recv_handler, ping_error_handler, 3, &stcbPing, &enErr);
        if (nRtnVal <= 0)
        {
            unErrCount++; 
            nvt_outputf(stcbPing.stArgs.ullNvtHandle, 256, "no reply received, the current number of errors is %d, current error: %s\r\n", unErrCount, nRtnVal ? onps_error(enErr) : "recv timeout");
        }

        os_sleep_secs(1); 
    } 

    unRecvedPackets = unSeqNum - unErrCount;
    nvt_outputf(stcbPing.stArgs.ullNvtHandle, 256, "%s ping statistics:\r\n%d packets transimitted, %d received, %0.2f%% packet loss, time %d ms\r\nrtt min/avg/max = %d/%0.2f/%d ms\r\n", 
                    stcbPing.stArgs.szDstIp, unSeqNum, unRecvedPackets, ((FLOAT)unErrCount / (FLOAT)unSeqNum) * 100, stcbPing.stStatistics.unRttTotal,
                    stcbPing.stStatistics.unRttMin, unRecvedPackets ? (FLOAT)stcbPing.stStatistics.unRttTotal / unRecvedPackets : 0, stcbPing.stStatistics.unRttMax);

    ping_end(nPing); 

__lblEnd: 
    nvt_cmd_exec_end(stcbPing.stArgs.ullNvtHandle);
    nvt_cmd_thread_end();
}
#endif //* #if NETTOOLS_TELNETSRV && NVTCMD_PING_EN
#endif //* #if NETTOOLS_PING

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
#define SYMBOL_GLOBALS
#include "net_tools/ping.h"
#undef SYMBOL_GLOBALS

#if NETTOOLS_PING
INT ping_start(EN_ONPSERR *penErr)
{    
    return onps_input_new(AF_INET, IPPROTO_ICMP, penErr);
}

void ping_end(INT nPing)
{
    onps_input_free(nPing);
}

INT ping_send(INT nPing, in_addr_t unDstAddr, USHORT usSeqNum, UCHAR ubTTL, const UCHAR *pubEcho, UCHAR ubEchoLen, EN_ONPSERR *penErr)
{    
    return icmp_send_echo_reqest(nPing, (USHORT)(nPing + 1), usSeqNum, ubTTL, unDstAddr, pubEcho, (UINT)ubEchoLen, penErr); 
}

INT ping_recv(INT nPing, in_addr_t *punFromAddr, USHORT *pusSeqNum, UCHAR *pubDataBuf, UCHAR ubDataBufSize, UCHAR *pubTTL, UCHAR ubWaitSecs, EN_ONPSERR *penErr)
{
    UCHAR *pubPacket; 

    INT nRcvedBytes = onps_input_recv_icmp(nPing, &pubPacket, punFromAddr, pubTTL, ubWaitSecs); 
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
INT ping(INT nPing, in_addr_t unDstAddr, USHORT usSeqNum, UCHAR ubTTL, UINT(*pfunGetCurMSecs)(void), void(*pfunRcvHandler)(USHORT usIdentifier, in_addr_t unFromAddr, USHORT usSeqNum, UCHAR *pubEchoData, UCHAR ubEchoDataLen, UCHAR ubTTL, UCHAR ubElapsedMSecs), UCHAR ubWaitSecs, EN_ONPSERR *penErr)
{
    UINT unStartMillisecs = pfunGetCurMSecs(); 
    INT nRtnVal = ping_send(nPing, unDstAddr, usSeqNum, ubTTL, "I am Trinity, Neo. Welcome to zion.\x00", strlen("I am Trinity, Neo. Welcome to zion.\x00") + 1, penErr); 
    if (nRtnVal < 0)
        return -1; 

    in_addr_t unFromAddr; 
    USHORT usReplySeqNum; 
    UCHAR ubReplyTTL; 
    UCHAR ubaEchoData[48]; 
    INT nRcvedBytes = ping_recv(nPing, &unFromAddr, &usReplySeqNum, ubaEchoData, sizeof(ubaEchoData) - 1, &ubReplyTTL, ubWaitSecs, penErr);
    if (nRcvedBytes > 0)
    {
        ubaEchoData[nRcvedBytes] = 0; 
        pfunRcvHandler(nPing + 1, unFromAddr, usReplySeqNum, ubaEchoData, (UCHAR)nRcvedBytes, ubReplyTTL, (UCHAR)(pfunGetCurMSecs() - unStartMillisecs)); 
    }

    return nRcvedBytes; 
}
#endif

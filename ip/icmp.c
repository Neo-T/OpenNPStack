#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "onps_input.h"
#define SYMBOL_GLOBALS
#include "ip/icmp.h"
#undef SYMBOL_GLOBALS

static INT icmp_send(in_addr_t unDstAddr, EN_ICMPTYPE enType, UCHAR ubCode, UCHAR ubTTL, SHORT sBufListHead, EN_ONPSERR *penErr)
{
    //* 填充头部字段
    ST_ICMP_HDR stHdr; 
    stHdr.ubType = (UCHAR)enType; 
    stHdr.ubCode = ubCode;   
    stHdr.usChecksum = 0;  //* 该字段必须为0，因为校验和计算范围覆盖该字段，其初始值必须为0才可
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (USHORT)sizeof(ST_ICMP_HDR), penErr);
    if (sHdrNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sHdrNode);

    //* 计算校验和
    stHdr.usChecksum = tcpip_checksum_ext(sBufListHead); 

    //* 完成发送
    INT nRtnVal = ip_send(unDstAddr, ICMP, ubTTL, sBufListHead, penErr);

    //* 释放刚才申请的buf list节点
    buf_list_free(sHdrNode);

    return nRtnVal; 
}

INT icmp_send_echo_reqest(INT nInput, USHORT usIdentifier, USHORT usSeqNum, UCHAR ubTTL, UINT unDstAddr, UCHAR *pubData, UINT unDataSize, EN_ONPSERR *penErr)
{
    //* 申请一个buf list节点
    SHORT sBufListHead = -1;
    SHORT sDataNode = buf_list_get_ext(pubData, (USHORT)unDataSize, penErr);
    if (sDataNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sDataNode);

    ST_ICMP_ECHO_HDR stEchoHdr; 
    stEchoHdr.usIdentifier = usIdentifier; 
    stEchoHdr.usSeqNum = usSeqNum; 
    //* 挂载到buf list头部
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stEchoHdr, (USHORT)sizeof(ST_ICMP_ECHO_HDR), penErr);
    if (sHdrNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sHdrNode);

    //* 记录echo identifier，以便区分echo应答报文
    INT nRtnVal;    
    if (onps_input_set(nInput, IOPT_SETICMPECHOID, &usIdentifier, penErr))    
        nRtnVal = icmp_send(unDstAddr, ICMP_ECHOREQ, 0, ubTTL, sBufListHead, penErr);     
    else
    {
#if SUPPORT_PRINTF
        printf("onps_input_set() failed (the option is IOPT_SETICMPECHOID), %s\r\n", onps_error(*penErr)); 
#endif
    }

    //* 释放刚才申请的buf list节点
    buf_list_free(sDataNode);
    buf_list_free(sHdrNode);

    return nRtnVal; 
}

static void icmp_rcv_handler_echoreply(UCHAR *pubPacket, INT nPacketLen, UCHAR ubTTL)
{
    PST_ICMP_ECHO_HDR pstEchoHdr = (PST_ICMP_ECHO_HDR)(pubPacket + sizeof(ST_ICMP_HDR));
    INT nInput = onps_input_get_icmp(pstEchoHdr->usIdentifier);
    if (nInput < 0)
    {
#if SUPPORT_PRINTF
        printf("The icmp echo request packet with ID %d is not found, the packet will be dropped\r\n", pstEchoHdr->usIdentifier);
#endif
        return;
    }

    //* 将数据搬运到用户的接收缓冲区，然后发送一个信号量通知用户数据已到达
    EN_ONPSERR enErr; 
    if (!onps_input_set(nInput, IOPT_SETICMPECHOREPTTL, &ubTTL, &enErr))
    {
#if SUPPORT_PRINTF
        printf("onps_input_set() failed (the option is IOPT_SETICMPECHOREPTTL), %s\r\n", onps_error(enErr)); 
#endif
    }

    HSEM hSem;
    UINT unRcvedBytes = nPacketLen - sizeof(ST_ICMP_HDR);
    UCHAR *pubRcvBuf = onps_input_get_rcv_buf(nInput, &hSem, &unRcvedBytes);
    memcpy(pubRcvBuf, pubPacket + sizeof(ST_ICMP_HDR), unRcvedBytes);
    os_thread_sem_post(hSem);
}

void icmp_recv(UCHAR *pubPacket, INT nPacketLen, UCHAR ubTTL)
{
    PST_ICMP_HDR pstHdr = (PST_ICMP_HDR)pubPacket; 

    //* 先看看校验和是否正确
    USHORT usPktChecksum = pstHdr->usChecksum;
    pstHdr->usChecksum = 0;
    USHORT usChecksum = tcpip_checksum((USHORT *)pubPacket, nPacketLen);
    if (usPktChecksum != usChecksum)
    {
#if SUPPORT_PRINTF
        printf("checksum error (%04X, %04X), and the icmp packet will be dropped\r\n", usChecksum, usPktChecksum);
#endif
        return;
    }

    switch ((EN_ICMPTYPE)pstHdr->ubType)
    {
    case ICMP_ECHOREPLY: 
        icmp_rcv_handler_echoreply(pubPacket, nPacketLen, ubTTL); 
        break; 

    default:
#if SUPPORT_PRINTF
        printf("Unsupported icmp packet type (%d), and the packet will be dropped\r\n", (UINT)pstHdr->ubType); 
#endif
        break;
    }
}

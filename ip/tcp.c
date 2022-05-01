#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "onps_input.h"
#include "ip/tcp_link.h"
#include "ip/tcp_frame.h"
#include "ip/tcp_options.h" 
#define SYMBOL_GLOBALS
#include "ip/tcp.h"
#undef SYMBOL_GLOBALS

static INT tcp_send_packet(in_addr_t unSrcAddr, USHORT usSrcPort, in_addr_t unDstAddr, USHORT usDstPort, UINT unSeqNum, UINT unAckNum, 
                            UNI_TCP_FLAG uniFlag, USHORT usWndSize, UCHAR *pubOptions, USHORT usOptionsBytes, UCHAR *pubData, USHORT usDataBytes, EN_ONPSERR *penErr)
{
    //* 挂载用户数据
    SHORT sBufListHead = -1; 
    SHORT sDataNode = -1; 
    if (pubData)
    {        
        sDataNode = buf_list_get_ext(pubData, usDataBytes, penErr); 
        if (sDataNode < 0)
            return -1;
        buf_list_put_head(&sBufListHead, sDataNode);
    }

    //* 挂载tcp options选项
    SHORT sOptionsNode = -1; 
    if (usOptionsBytes)
    {
        sOptionsNode = buf_list_get_ext(pubOptions, usOptionsBytes, penErr);
        if (sOptionsNode < 0)
        {
            if (sDataNode >= 0)
                buf_list_free(sDataNode);

            return -1;
        }
        buf_list_put_head(&sBufListHead, sOptionsNode);
    }

    //* 填充tcp头
    ST_TCP_HDR stHdr; 
    stHdr.usSrcPort = htons(usSrcPort);
    stHdr.usDstPort = htons(usDstPort);
    stHdr.unSeqNum = htonl(unSeqNum);
    stHdr.unAckNum = htonl(unAckNum);
    uniFlag.stb16.hdr_len = (UCHAR)(sizeof(ST_TCP_HDR) / 4) + (UCHAR)(usOptionsBytes / 4); //* TCP头部字段实际长度（单位：32位整型）
    stHdr.uniFlag = uniFlag;
    stHdr.usWinSize = htons(usWndSize);
    stHdr.usChecksum = 0;
    stHdr.usUrgentPointer = 0; 
    //* 挂载到链表头部
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (USHORT)sizeof(ST_TCP_HDR), penErr);
    if (sHdrNode < 0)
    {
        if (sDataNode >= 0)
            buf_list_free(sDataNode);
        if (sOptionsNode >= 0)
            buf_list_free(sOptionsNode);

        return -1;
    }
    buf_list_put_head(&sBufListHead, sHdrNode); 

    //* 填充用于校验和计算的tcp伪报头
    ST_TCP_PSEUDOHDR stPseudoHdr; 
    stPseudoHdr.unSrcAddr = unSrcAddr;
    stPseudoHdr.unDestAddr = unDstAddr; 
    stPseudoHdr.ubMustBeZero = 0; 
    stPseudoHdr.ubProto = IPPROTO_TCP; 
    stPseudoHdr.usPacketLen = htons(sizeof(ST_TCP_HDR) + usOptionsBytes + usDataBytes); 
    //* 挂载到链表头部
    SHORT sPseudoHdrNode;
    sPseudoHdrNode = buf_list_get_ext((UCHAR *)&stPseudoHdr, (USHORT)sizeof(ST_TCP_PSEUDOHDR), penErr);
    if (sPseudoHdrNode < 0)
    {
        if (sDataNode >= 0)
            buf_list_free(sDataNode);
        if (sOptionsNode >= 0)
            buf_list_free(sOptionsNode);
        buf_list_free(sHdrNode);

        return -1;
    }
    buf_list_put_head(&sBufListHead, sPseudoHdrNode);

    //* 计算校验和
    stHdr.usChecksum = tcpip_checksum_ext(sBufListHead); 
    //* 用不到了，释放伪报头
    buf_list_free(sPseudoHdrNode);

    //* 发送之
    INT nRtnVal = ip_send_ext(unSrcAddr, unDstAddr, TCP, IP_TTL_DEFAULT, sBufListHead, penErr);

    //* 释放刚才申请的buf list节点
    if(sDataNode >= 0)
        buf_list_free(sDataNode); 
    if(sOptionsNode >= 0)
        buf_list_free(sOptionsNode); 
    buf_list_free(sHdrNode);

    return nRtnVal; 
}

INT tcp_send_syn(INT nInput, in_addr_t unSrvAddr, USHORT usSrvPort)
{
    EN_ONPSERR enErr;    

    //* 获取链路信息存储节点
    PST_TCPLINK pstLink; 
    if (!onps_input_get(nInput, IOPT_GETATTACH, &pstLink, &enErr))
    {
        onps_set_last_error(nInput, enErr); 
        return -1; 
    }
    pstLink->usWndSize = TCPRCVBUF_SIZE_DEFAULT - sizeof(ST_TCP_HDR) - TCP_OPTIONS_SIZE_MAX;

    //* 获取tcp链路句柄访问地址，该地址保存当前tcp链路由协议栈自动分配的端口及本地网络接口地址
    PST_TCPUDP_HANDLE pstHandle; 
    if (!onps_input_get(nInput, IOPT_GETTCPUDPADDR, &pstHandle, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return -1;
    }

    //* 先寻址，因为tcp校验和计算需要用到本地地址，同时当前tcp链路句柄也需要用此标识
    UINT unNetifIp = route_get_netif_ip(unSrvAddr);
    if (!unNetifIp)
    {
        onps_set_last_error(nInput, ERRADDRESSING);
        return -1;
    }
    //* 更新当前input句柄，以便收到应答报文时能够准确找到该链路
    pstHandle->unNetifIp = unNetifIp;
    pstHandle->usPort = onps_input_port_new(IPPROTO_TCP); 

    //* 标志字段syn域置1，其它标志域为0
    UNI_TCP_FLAG uniFlag; 
    uniFlag.usVal = 0; 
    uniFlag.stb16.syn = 1;        

    //* 填充tcp头部选项数据
    UCHAR ubaOptions[TCP_OPTIONS_SIZE_MAX]; 
    INT nOptionsSize = tcp_options_attach(ubaOptions, sizeof(ubaOptions));    

    //* 完成实际的发送
    INT nRtnVal = tcp_send_packet(pstHandle->unNetifIp, pstHandle->usPort, unSrvAddr, usSrvPort, pstLink->unSeqNum, pstLink->unAckNum, 
                                    uniFlag, pstLink->usWndSize, ubaOptions, (USHORT)nOptionsSize, NULL, 0, &enErr); 
    if (nRtnVal < 0)
        onps_set_last_error(nInput, enErr);
    return nRtnVal; 
}

void tcp_recv(in_addr_t unSrcAddr, in_addr_t unDstAddr, UCHAR *pubPacket, INT nPacketLen)
{
    PST_TCP_HDR pstHdr = (PST_TCP_HDR)pubPacket; 
    
    //* 把完整的tcp报文与tcp伪包头链接到一起，以便计算tcp校验和确保收到的tcp报文正确
    EN_ONPSERR enErr; 
    SHORT sBufListHead = -1;
    SHORT sTcpPacketNode = -1;
    sTcpPacketNode = buf_list_get_ext(pubPacket, nPacketLen, &enErr);
    if (sTcpPacketNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sTcpPacketNode);

    //* 填充用于校验和计算的tcp伪报头
    ST_TCP_PSEUDOHDR stPseudoHdr;
    stPseudoHdr.unSrcAddr = unSrcAddr;
    stPseudoHdr.unDestAddr = unDstAddr;
    stPseudoHdr.ubMustBeZero = 0;
    stPseudoHdr.ubProto = IPPROTO_TCP;
    stPseudoHdr.usPacketLen = htons((USHORT)nPacketLen); 
    //* 挂载到链表头部
    SHORT sPseudoHdrNode;
    sPseudoHdrNode = buf_list_get_ext((UCHAR *)&stPseudoHdr, (USHORT)sizeof(ST_TCP_PSEUDOHDR), &enErr);
    if (sPseudoHdrNode < 0)
    {        
        buf_list_free(sTcpPacketNode); 
        return -1;
    }
    buf_list_put_head(&sBufListHead, sPseudoHdrNode);

    //* 挂载完毕，可以计算校验和是否正确了
    USHORT usPktChecksum = pstHdr->usChecksum;
    pstHdr->usChecksum = 0;
    USHORT usChecksum = tcpip_checksum_ext(sBufListHead);
    //* 先释放再判断
    buf_list_free(sTcpPacketNode);
    buf_list_free(sPseudoHdrNode);
    if (usPktChecksum != usChecksum)
    {
#if SUPPORT_PRINTF
        pstHdr->usChecksum = usPktChecksum;
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("checksum error (%04X, %04X), the tcp packet will be dropped\r\n", usChecksum, usPktChecksum);
        printf_hex(pubPacket, nPacketLen, 48);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }

    //* 先查找当前链路是否存在
    USHORT usDstPort = htons(pstHdr->usDstPort);
    INT nInput = onps_input_get_handle(unDstAddr, usDstPort);
    if (nInput < 0)
    {
#if SUPPORT_PRINTF
        pstHdr->usChecksum = usPktChecksum;
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        UCHAR *pubAddr = (UCHAR *)&unDstAddr;
        printf("The tcp link of %d.%d.%d.%d:%d isn't found, the packet will be dropped\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3], usDstPort);
        printf_hex(pubPacket, nPacketLen, 48);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return; 
    }

    
}


#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "one_shot_timer.h"
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

static void tcp_ack_timeout_handler(void *pvParam)
{
    PST_TCPLINK pstLink = (PST_TCPLINK)pvParam; 
    if (!pstLink->stcbWaitAck.bIsAcked)
    {
        pstLink->bState = TLSACKTIMEOUT; 
        os_thread_sem_post(pstLink->stcbWaitAck.hSem); 
    }
}

static INT tcp_send_packet(in_addr_t unSrcAddr, USHORT usSrcPort, in_addr_t unDstAddr, USHORT usDstPort, UINT unSeqNum, UINT unAckNum, 
                            UNI_TCP_FLAG uniFlag, USHORT usWndSize, UCHAR *pubOptions, USHORT usOptionsBytes, UCHAR *pubData, USHORT usDataBytes, EN_ONPSERR *penErr)
{
    //* 挂载用户数据
    SHORT sBufListHead = -1; 
    SHORT sDataNode = -1; 
    if (pubData)
    {        
        sDataNode = buf_list_get_ext(pubData, (UINT)usDataBytes, penErr); 
        if (sDataNode < 0)
            return -1;
        buf_list_put_head(&sBufListHead, sDataNode);
    }

    //* 挂载tcp options选项
    SHORT sOptionsNode = -1; 
    if (usOptionsBytes)
    {
        sOptionsNode = buf_list_get_ext(pubOptions, (UINT)usOptionsBytes, penErr);
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
    stHdr.usFlag = uniFlag.usVal;
    stHdr.usWinSize = htons(usWndSize);
    stHdr.usChecksum = 0;
    stHdr.usUrgentPointer = 0; 
    //* 挂载到链表头部
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (UINT)sizeof(ST_TCP_HDR), penErr);
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
    stPseudoHdr.unDestAddr = htonl(unDstAddr);
    stPseudoHdr.ubMustBeZero = 0; 
    stPseudoHdr.ubProto = IPPROTO_TCP; 
    stPseudoHdr.usPacketLen = htons(sizeof(ST_TCP_HDR) + usOptionsBytes + usDataBytes); 
    printf_hex((UCHAR *)&stPseudoHdr, sizeof(stPseudoHdr), 48);
    //* 挂载到链表头部
    SHORT sPseudoHdrNode;
    sPseudoHdrNode = buf_list_get_ext((UCHAR *)&stPseudoHdr, (UINT)sizeof(ST_TCP_PSEUDOHDR), penErr);
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
    buf_list_free_head(&sBufListHead, sPseudoHdrNode);

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

static void tcp_send_ack_of_syn_ack(INT nInput, PST_TCPLINK pstLink, in_addr_t unNetifIp, USHORT usSrcPort, UINT unSrvAckNum)
{
    //* 标志字段syn域置1，其它标志域为0
    UNI_TCP_FLAG uniFlag;
    uniFlag.usVal = 0;
    uniFlag.stb16.ack = 1;

    //* 更新tcp序号
    pstLink->unSeqNum = unSrvAckNum; 
    pstLink->unAckNum += 1; 

    //* 发送
    EN_ONPSERR enErr;
    INT nRtnVal = tcp_send_packet(unNetifIp, usSrcPort, pstLink->stPeerAddr.unIp, pstLink->stPeerAddr.usPort, pstLink->unSeqNum, pstLink->unAckNum,
                                    uniFlag, pstLink->usWndSize, NULL, 0, NULL, 0, &enErr);
    if (nRtnVal > 0)
    {
        //* 连接成功
        pstLink->bState = TLSCONNECTED; 
    }
    else 
    {
        pstLink->bState = TLSSYNACKACKSENTFAILED;

        if (nRtnVal < 0)
            onps_set_last_error(nInput, enErr);
        else
            onps_set_last_error(nInput, ERRSENDZEROBYTES);
    }
    os_thread_sem_post(pstLink->stcbWaitAck.hSem);
}

INT tcp_send_syn(INT nInput, HSEM hSem, in_addr_t unSrvAddr, USHORT usSrvPort)
{
    EN_ONPSERR enErr;    

    //* 获取链路信息存储节点
    PST_TCPLINK pstLink; 
    if (!onps_input_get(nInput, IOPT_GETATTACH, &pstLink, &enErr))
    {
        onps_set_last_error(nInput, enErr); 
        return -1; 
    }
    pstLink->usWndSize = 64240/*TCPRCVBUF_SIZE_DEFAULT - sizeof(ST_TCP_HDR) - TCP_OPTIONS_SIZE_MAX*/;

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
    if (nRtnVal > 0)
    {
        //* 加入定时器队列
        pstLink->stcbWaitAck.bIsAcked = FALSE; 
        pstLink->stcbWaitAck.hSem = hSem; 
        pstLink->stcbWaitAck.pstTimer = one_shot_timer_new(tcp_ack_timeout_handler, pstLink, TCP_CONN_TIMEOUT);
        if (!pstLink->stcbWaitAck.pstTimer)
        {
            onps_set_last_error(nInput, ERRNOIDLETIMER);
            return -1;             
        }
        pstLink->bState = TLSSYNSENT; //* 只有定时器申请成功了才会将链路状态迁移到syn报文已发送状态，以确保收到syn ack时能够进行正确匹配
    }
    else
    {
        if(nRtnVal < 0)
            onps_set_last_error(nInput, enErr);
        else
            onps_set_last_error(nInput, ERRSENDZEROBYTES);
    }

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
    {
#if SUPPORT_PRINTF        
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("buf_list_get_ext() failed, %s, the tcp packet will be dropped\r\n", onps_error(enErr));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }
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
#if SUPPORT_PRINTF        
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("buf_list_get_ext() failed, %s, the tcp packet will be dropped\r\n", onps_error(enErr));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif

        buf_list_free(sTcpPacketNode); 
        return;
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
    PST_TCPLINK pstLink; 
    INT nInput = onps_input_get_handle_ext(unDstAddr, usDstPort, &pstLink);
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

    //* 依据报文头部标志字段确定下一步的处理逻辑
    UNI_TCP_FLAG uniFlag; 
    uniFlag.usVal = pstHdr->usFlag;
    if (uniFlag.stb16.ack)
    {
        //* 连接请求的应答报文
        if (uniFlag.stb16.syn)
        {
            if (TLSSYNSENT == pstLink->bState && pstHdr->unAckNum == pstLink->unSeqNum + 1) //* 确定这是一个有效的syn ack报文才可进入下一个处理流程，否则报文将被直接抛弃
            {
                pstLink->stcbWaitAck.bIsAcked = TRUE; 
                one_shot_timer_recount(pstLink->stcbWaitAck.pstTimer, 1); //* 通知定时器结束计时，释放占用的非常宝贵的定时器资源

                //* 记录当前链路信息
                pstLink->unAckNum = pstHdr->unSeqNum; 
                pstLink->usWndSize = pstHdr->usWinSize; 
                pstLink->stPeerAddr.unIp = unSrcAddr; 
                pstLink->stPeerAddr.usPort = pstHdr->usSrcPort; 

                //* 截取tcp头部选项字段
                tcp_options_get(pstLink, pubPacket + sizeof(ST_TCP_HDR), uniFlag.stb16.hdr_len * 4 - sizeof(ST_TCP_HDR));

                //* 状态迁移到已接收到syn ack报文
                pstLink->bState = TLSRCVEDSYNACK;

                //* 发送syn ack的ack报文
                tcp_send_ack_of_syn_ack(nInput, pstLink, unDstAddr, usDstPort, pstHdr->unAckNum);
            }
        }
    }
}


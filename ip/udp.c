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
#define SYMBOL_GLOBALS
#include "ip/udp.h"
#include "ip/udp_frame.h" 
#undef SYMBOL_GLOBALS

static INT udp_send_packet(in_addr_t unSrcAddr, USHORT usSrcPort, in_addr_t unDstAddr, USHORT usDstPort, UCHAR *pubData, INT nDataLen, EN_ONPSERR *penErr)
{
    //* 挂载用户数据
    SHORT sBufListHead = -1;
    SHORT sDataNode = -1;
    if (pubData)
    {
        sDataNode = buf_list_get_ext(pubData, (UINT)nDataLen, penErr);
        if (sDataNode < 0)
            return -1;
        buf_list_put_head(&sBufListHead, sDataNode);
    }

    //* 挂载头部数据
    ST_UDP_HDR stHdr;     
    stHdr.usSrcPort = htons(usSrcPort);
    stHdr.usDstPort = htons(usDstPort);
    stHdr.usPacketLen = htons(sizeof(ST_UDP_HDR) + nDataLen); 
    stHdr.usChecksum = 0; 

    //* 挂载到链表头部
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (UINT)sizeof(ST_UDP_HDR), penErr);
    if (sHdrNode < 0)
    {
        buf_list_free(sDataNode);         
        return -1;
    }
    buf_list_put_head(&sBufListHead, sHdrNode);

    //* 填充用于校验和计算的tcp伪报头
    ST_UDP_PSEUDOHDR stPseudoHdr;
    stPseudoHdr.unSrcAddr = unSrcAddr;
    stPseudoHdr.unDestAddr = htonl(unDstAddr);
    stPseudoHdr.ubMustBeZero = 0;
    stPseudoHdr.ubProto = IPPROTO_UDP;
    stPseudoHdr.usPacketLen = htons(sizeof(ST_UDP_HDR) + nDataLen);     
    //* 挂载到链表头部
    SHORT sPseudoHdrNode;
    sPseudoHdrNode = buf_list_get_ext((UCHAR *)&stPseudoHdr, (UINT)sizeof(ST_UDP_PSEUDOHDR), penErr);
    if (sPseudoHdrNode < 0)
    {
        buf_list_free(sDataNode);        
        buf_list_free(sHdrNode);
        return -1;
    }
    buf_list_put_head(&sBufListHead, sPseudoHdrNode);

    //* 计算校验和
    stHdr.usChecksum = tcpip_checksum_ext(sBufListHead);
    //* 用不到了，释放伪报头
    buf_list_free_head(&sBufListHead, sPseudoHdrNode);

    //* 如果校验和为0，根据协议要求必须反转为0xFFFF
    if (0 == stHdr.usChecksum)
        stHdr.usChecksum = 0xFFFF; 

    //* 发送之
    INT nRtnVal = ip_send_ext(unSrcAddr, unDstAddr, UDP, IP_TTL_DEFAULT, sBufListHead, penErr);

    //* 释放刚才申请的buf list节点
    buf_list_free(sDataNode);    
    buf_list_free(sHdrNode);

    return nRtnVal;
}

INT udp_send(INT nInput, UCHAR *pubData, INT nDataLen)
{
    EN_ONPSERR enErr;
    
    //* 获取链路信息存储节点
    PST_UDPLINK pstLink;
    if (!onps_input_get(nInput, IOPT_GETATTACH, &pstLink, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return -1;
    }

    //* 如果未绑定对端服务器地址，或者地址/端口为空则报错
    if (pstLink == NULL || pstLink->stPeerAddr.unIp == 0 || pstLink->stPeerAddr.usPort == 0)
    {
        onps_set_last_error(nInput, ERRSENDADDR);
        return -1; 
    }

    //* 获取tcp链路句柄访问地址，该地址保存当前tcp链路由协议栈自动分配的端口及本地网络接口地址
    PST_TCPUDP_HANDLE pstHandle;
    if (!onps_input_get(nInput, IOPT_GETTCPUDPADDR, &pstHandle, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return -1;
    }

    //* 尚未分配本地地址
    if (pstHandle->unNetifIp == 0 && pstHandle->usPort == 0)
    {
        //* 寻址，看看使用哪个neiif
        UINT unNetifIp = route_get_netif_ip(pstLink->stPeerAddr.unIp);
        if (!unNetifIp)
        {
            onps_set_last_error(nInput, ERRADDRESSING);
            return -1;
        }
        //* 更新当前input句柄，以便收到应答报文时能够准确找到该链路
        pstHandle->unNetifIp = unNetifIp;
        pstHandle->usPort = onps_input_port_new(IPPROTO_UDP);
    }

    return udp_send_packet(pstHandle->unNetifIp, pstHandle->usPort, pstLink->stPeerAddr.unIp, pstLink->stPeerAddr.usPort, pubData, nDataLen, &enErr); 
}

INT udp_sendto(INT nInput, in_addr_t unDstIP, USHORT usDstPort, UCHAR *pubData, INT nDataLen)
{
    EN_ONPSERR enErr;

    //* 获取tcp链路句柄访问地址，该地址保存当前tcp链路由协议栈自动分配的端口及本地网络接口地址
    PST_TCPUDP_HANDLE pstHandle;
    if (!onps_input_get(nInput, IOPT_GETTCPUDPADDR, &pstHandle, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return -1;
    }

    //* 尚未分配本地地址(一定是IP地址和端口号都为0才可，因为存在UDP服务器只绑定一个端口号的情况)
    if (pstHandle->unNetifIp == 0 && pstHandle->usPort == 0)
    {
        //* 寻址，看看使用哪个neiif
        UINT unNetifIp = route_get_netif_ip(unDstIP);
        if (!unNetifIp)
        {
            onps_set_last_error(nInput, ERRADDRESSING);
            return -1;
        }
        //* 更新当前input句柄，以便收到应答报文时能够准确找到该链路
        pstHandle->unNetifIp = unNetifIp;
        pstHandle->usPort = onps_input_port_new(IPPROTO_UDP);
    }

    return udp_send_packet(pstHandle->unNetifIp, pstHandle->usPort, unDstIP, usDstPort, pubData, nDataLen, &enErr); 
}

void udp_recv(in_addr_t unSrcAddr, in_addr_t unDstAddr, UCHAR *pubPacket, INT nPacketLen)
{
    EN_ONPSERR enErr;
    PST_UDP_HDR pstHdr = (PST_UDP_HDR)pubPacket; 
    
    //* 如果校验和为0则意味着不需要计算校验和，反之就需要进行校验计算
    if (pstHdr->usChecksum)
    {
        //* 把完整的tcp报文与tcp伪包头链接到一起，以便计算tcp校验和确保收到的tcp报文正确        
        SHORT sBufListHead = -1;
        SHORT sUdpPacketNode = -1;
        sUdpPacketNode = buf_list_get_ext(pubPacket, nPacketLen, &enErr);
        if (sUdpPacketNode < 0)
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
        buf_list_put_head(&sBufListHead, sUdpPacketNode);

        //* 填充用于校验和计算的udp伪报头
        ST_UDP_PSEUDOHDR stPseudoHdr;
        stPseudoHdr.unSrcAddr = unSrcAddr;
        stPseudoHdr.unDestAddr = unDstAddr;
        stPseudoHdr.ubMustBeZero = 0;
        stPseudoHdr.ubProto = IPPROTO_UDP;
        stPseudoHdr.usPacketLen = htons((USHORT)nPacketLen);
        //* 挂载到链表头部
        SHORT sPseudoHdrNode;
        sPseudoHdrNode = buf_list_get_ext((UCHAR *)&stPseudoHdr, (USHORT)sizeof(ST_UDP_PSEUDOHDR), &enErr);
        if (sPseudoHdrNode < 0)
        {
    #if SUPPORT_PRINTF        
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("buf_list_get_ext() failed, %s, the udp packet will be dropped\r\n", onps_error(enErr));
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif

            buf_list_free(sUdpPacketNode);
            return;
        }
        buf_list_put_head(&sBufListHead, sPseudoHdrNode);

        //* 挂载完毕，可以计算校验和是否正确了
        USHORT usPktChecksum = pstHdr->usChecksum;
        pstHdr->usChecksum = 0;
        USHORT usChecksum = tcpip_checksum_ext(sBufListHead);
        //* 先释放再判断
        buf_list_free(sUdpPacketNode);
        buf_list_free(sPseudoHdrNode);
        if(0 == usChecksum) //* 如果计算结果为0，则校验和反转
            usChecksum = 0xFFFF; 
        if (usPktChecksum != usChecksum)
        {
    #if SUPPORT_PRINTF
            pstHdr->usChecksum = usPktChecksum;
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("checksum error (%04X, %04X), the udp packet will be dropped\r\n", usChecksum, usPktChecksum);
            printf_hex(pubPacket, nPacketLen, 48);
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
#endif
            return;
        }
    }

    //* 校验和计算通过，则查找当前udp链路是否存在
    USHORT usSrcPort = htons(pstHdr->usSrcPort); 
    USHORT usDstPort = htons(pstHdr->usDstPort);
    PST_UDPLINK pstLink;
    INT nInput = onps_input_get_handle_ext(unDstAddr, usDstPort, &pstLink); 
    if (nInput < 0)
    {
#if SUPPORT_PRINTF        
        UCHAR *pubAddr = (UCHAR *)&unDstAddr;
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif        
        printf("The udp link of %d.%d.%d.%d:%d isn't found, the packet will be dropped\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3], usDstPort);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }

    //* 如果当前链路存在附加信息，意味着这是一个已经与udp服务器绑定的链路，需要判断源地址是否也匹配，不匹配的直接丢弃
    in_addr_t unFromIP = htonl(unSrcAddr); 
    if (pstLink)
    {
        if (pstLink->stPeerAddr.unIp != unFromIP || pstLink->stPeerAddr.usPort != usSrcPort)
        {
    #if SUPPORT_PRINTF        
            UCHAR *pubFromAddr = (UCHAR *)&unSrcAddr;
            UCHAR *pubSrvAddr = (UCHAR *)&pstLink->stPeerAddr.unIp;
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif        
            printf("udp packets from address %d.%d.%d.%d:%d are not allowed (connected to udp server %d.%d.%d.%d:%d), the packet will be dropped\r\n", pubFromAddr[0], pubFromAddr[1], pubFromAddr[2], pubFromAddr[3], usSrcPort, pubSrvAddr[3], pubSrvAddr[2], pubSrvAddr[1], pubSrvAddr[0], pstLink->stPeerAddr.usPort);
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif
            return;
        }
    }

    //* 看看有数据吗？            
    INT nDataLen = nPacketLen - sizeof(ST_UDP_HDR);
    if (nDataLen)
    {
        //* 将数据搬运到input层
        if (!onps_input_recv(nInput, (const UCHAR *)(pubPacket + sizeof(ST_UDP_HDR)), nDataLen, unFromIP, usSrcPort, &enErr))
        {
    #if SUPPORT_PRINTF
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("onps_input_recv() failed, %s, the tcp packet will be dropped\r\n", onps_error(enErr));
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif            
        }
    }    
}

INT udp_recv_upper(INT nInput, UCHAR *pubDataBuf, UINT unDataBufSize, CHAR bRcvTimeout)
{
    EN_ONPSERR enErr;
    INT nRcvedBytes;

    //* 读取数据
    nRcvedBytes = onps_input_recv_upper(nInput, pubDataBuf, unDataBufSize, NULL, NULL, &enErr);
    if (nRcvedBytes > 0)
        return nRcvedBytes;
    else
    {
        if (nRcvedBytes < 0)
            goto __lblErr;
    }

    //* 等待后
    if (bRcvTimeout)
    {
        CHAR bWaitSecs;        

    __lblWaitRecv:
        bWaitSecs = bRcvTimeout;
        if (bRcvTimeout > 0)
        {
            if (onps_input_sem_pend(nInput, 1, &enErr) < 0)            
                goto __lblErr;            

            bWaitSecs--;
        }
        else
        {
            if (onps_input_sem_pend(nInput, 0, &enErr) < 0)            
                goto __lblErr;           
        }

        //* 读取数据
        nRcvedBytes = onps_input_recv_upper(nInput, pubDataBuf, unDataBufSize, NULL, NULL, &enErr);
        if (nRcvedBytes > 0)
            return nRcvedBytes;
        else
        {
            if (nRcvedBytes < 0)
                goto __lblErr;

            if (bWaitSecs <= 0)
                return 0;
            goto __lblWaitRecv;
        }
    }
    else
        return 0;


__lblErr:
    onps_set_last_error(nInput, enErr);
    return -1;
}


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

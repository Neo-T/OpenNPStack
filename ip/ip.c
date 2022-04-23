#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "netif/netif.h"
#include "netif/route.h"

#define SYMBOL_GLOBALS
#include "ip/ip.h"
#undef SYMBOL_GLOBALS
#include "ip/icmp.h"

//* 必须严格按照EN_NPSPROTOCOL类型定义的顺序指定IP上层协议值
static const EN_IPPROTO lr_enaIPProto[] = {
    IPPROTO_MAX,
    IPPROTO_MAX,
    IPPROTO_MAX,
    IPPROTO_MAX,
#if SUPPORT_IPV6
    IPPROTO_MAX,
#endif
    IPPROTO_MAX,
#if SUPPORT_IPV6
    IPPROTO_MAX,
#endif
    IPPROTO_ICMP, 
    IPPROTO_MAX,
    IPPROTO_TCP, 
    IPPROTO_UDP
};
static USHORT l_usIPIdentifier = 0; 

INT ip_send(in_addr_t unDstAddr, EN_NPSPROTOCOL enProtocol, UCHAR ubTTL, SHORT sBufListHead, EN_ONPSERR *penErr)
{
    PST_NETIF pstNetif = route_get_netif(unDstAddr, TRUE);
    if (NULL == pstNetif)
        return -1; 

    os_critical_init();
    ST_IP_HDR stHdr; 
    stHdr.bitHdrLen = sizeof(ST_IP_HDR) / sizeof(UINT); //* IP头长度，单位：UINT
    stHdr.bitVer = 4; //* IPv4
    stHdr.bitMustBeZero = 0; 
    stHdr.bitTOS = 0; //* 一般服务
    stHdr.bitPrior = 0; 
    stHdr.usPacketLen = htons(sizeof(ST_IP_HDR) + (USHORT)buf_list_get_len(sBufListHead)); 
    os_enter_critical();
    {
        stHdr.usID = htons(l_usIPIdentifier);
        l_usIPIdentifier++;
    }    
    os_exit_critical(); 
    stHdr.bitFragOffset0 = 0;
    stHdr.bitFlag = 1 << 1;  //* Don't fragment
    stHdr.bitFragOffset1 = 0;
    stHdr.ubTTL = ubTTL;
    stHdr.ubProto = (UCHAR)lr_enaIPProto[enProtocol];
    stHdr.usChecksum = 0;
    stHdr.unSrcIP = pstNetif->stIPv4.unAddr; 
    stHdr.unDstIP = htonl(unDstAddr); 

    //* 挂载到buf list头部
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (USHORT)sizeof(ST_IP_HDR), penErr);
    if (sHdrNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sHdrNode);

    //* 计算校验和
    stHdr.usChecksum = tcpip_checksum_ext(sBufListHead);       
    
    //* 完成发送
    INT nRtnVal = pstNetif->pfunSend(pstNetif, IPV4, sBufListHead, penErr);           

    //* 使用计数减一
    os_enter_critical();
    {
        pstNetif->bUsedCount--; 
    }
    os_exit_critical(); 

    //* 释放刚才申请的buf list节点
    buf_list_free(sHdrNode);

    return nRtnVal; 
}

void ip_recv(UCHAR *pubPacket, INT nPacketLen)
{
    PST_IP_HDR pstHdr = (PST_IP_HDR)pubPacket;

    //* 首先看看校验和是否正确
    USHORT usPktChecksum = pstHdr->usChecksum;
    pstHdr->usChecksum = 0;
    USHORT usChecksum = tcpip_checksum((USHORT *)pubPacket, nPacketLen);
    if (usPktChecksum != usChecksum)
    {
#if SUPPORT_PRINTF
        pstHdr->usChecksum = usPktChecksum;
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("checksum error (%04X, %04X), and the IP packet will be dropped\r\n", usChecksum, usPktChecksum);
        printf_hex(pubPacket, nPacketLen, 48);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return; 
    }

    switch (pstHdr->ubProto)
    {
    case IPPROTO_ICMP: 
        icmp_recv(pubPacket, nPacketLen);
        break; 

    default:
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("unsupported IP upper layer protocol (%d), the packet will be dropped\r\n", (UINT)pstHdr->ubProto);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        break; 
    }
}

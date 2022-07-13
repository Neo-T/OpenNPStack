#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "ip/ip.h"

#if SUPPORT_ETHERNET
#include "ethernet/arp_frame.h"
#include "ethernet/ethernet.h"
#define SYMBOL_GLOBALS
#include "ethernet/arp.h"
#undef SYMBOL_GLOBAL

//* ethernet ii协议arp缓存表及最近读取使用的地址条目
static STCB_ETHARP l_stcbaEthArp[ETHERNET_NUM]; 

//* arp初始化
void arp_init(void)
{
    memset(l_stcbaEthArp, 0, sizeof(l_stcbaEthArp));
}

//* 获取一个新的arp控制块
PSTCB_ETHARP arp_ctl_block_new(void)
{
    PSTCB_ETHARP pstcbArp = NULL;
    os_critical_init();

    //* 申请一个新的控制块    
    os_enter_critical();
    {
        INT i; 
        for (i = 0; i < ETHERNET_NUM; i++)
        {
            if (!l_stcbaEthArp[i].bIsUsed)
            {
                l_stcbaEthArp[i].bIsUsed = TRUE;
                pstcbArp = &l_stcbaEthArp[i]; 
                pstcbArp->bLastEntryIPv4ToRead = 0;                 
                break; 
            }
        }
    }
    os_exit_critical();

    return pstcbArp; 
}

//*  归还占用的arp控制块
void arp_ctl_block_free(PSTCB_ETHARP pstcbArp)
{
    memset(pstcbArp, 0, sizeof(STCB_ETHARP));
    pstcbArp->bIsUsed = FALSE; 
}

//* 添加arp条目
void arp_add_ethii_ipv4(PST_ENTRY_ETHIIIPV4 pstArpIPv4Tbl, UINT unIPAddr, UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN])
{
    INT i; 

    os_critical_init(); 

    //* 先查找该条目是否已经存在，如果存在则直接更新，否则直接添加或替换最老条目
    for (i = 0; i < ARPENTRY_NUM; i++)
    {
        //* 至此，前面缓存的条目没有匹配的，不必继续查找了，直接新增即可
        if (!pstArpIPv4Tbl[i].unIPAddr)
            break;         

        if (unIPAddr == pstArpIPv4Tbl[i].unIPAddr) //* 匹配
        {
            //* 更新mac地址
            os_enter_critical();
            {
                memcpy(pstArpIPv4Tbl[i].ubaMacAddr, ubaMacAddr, ETH_MAC_ADDR_LEN);
                pstArpIPv4Tbl[i].unUpdateTime = os_get_system_secs();
            }
            os_exit_critical(); 
            return; 
        }
    }

    //* 到这里意味着尚未缓存该地址条目，需要增加一条或者覆盖最老的一个条目
    if (i >= ARPENTRY_NUM)    
    {
        INT nFirstEntry = 0; 
        for (i = 1; i < ARPENTRY_NUM; i++)
        {
            if (pstArpIPv4Tbl[nFirstEntry].unUpdateTime > pstArpIPv4Tbl[i].unUpdateTime)
                nFirstEntry = i;             
        }

        i = nFirstEntry; 
    }

    //* 更新mac地址
    os_enter_critical();
    {        
        memcpy(pstArpIPv4Tbl[i].ubaMacAddr, ubaMacAddr, ETH_MAC_ADDR_LEN);
        pstArpIPv4Tbl[i].unUpdateTime = os_get_system_secs();
        pstArpIPv4Tbl[i].unIPAddr = unIPAddr;
    }
    os_exit_critical();
}

INT arp_get_mac(PST_NETIF pstNetif, UINT unSrcIPAddr, UINT unDstArpIPAddr, UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], EN_ONPSERR *penErr)
{
    PSTCB_ETHARP pstcbArp = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbArp; 

    os_critical_init(); 

    //* 是否命中最近刚读取过的条目
    os_enter_critical();
    if (unDstArpIPAddr == pstcbArp->staEntryIPv4[pstcbArp->bLastEntryIPv4ToRead].unIPAddr) 
    {        
        memcpy(ubaMacAddr, pstcbArp->staEntryIPv4[pstcbArp->bLastEntryIPv4ToRead].ubaMacAddr, ETH_MAC_ADDR_LEN);
        pstcbArp->staEntryIPv4[pstcbArp->bLastEntryIPv4ToRead].unUpdateTime = os_get_system_secs();
        os_exit_critical(); 
        return 0; 
    }
    os_exit_critical();

    //* 未命中，则查找整个缓存表
    INT i; 
    for (i = 0; i < ARPENTRY_NUM; i++)
    {
        os_enter_critical();
        if (unDstArpIPAddr == pstcbArp->staEntryIPv4[i].unIPAddr)
        {
            memcpy(ubaMacAddr, pstcbArp->staEntryIPv4[i].ubaMacAddr, ETH_MAC_ADDR_LEN);
            pstcbArp->staEntryIPv4[i].unUpdateTime = os_get_system_secs();
            pstcbArp->bLastEntryIPv4ToRead = i;
            os_exit_critical();
            return 0;
        }
        os_exit_critical();
    }

    //* 不存在，则只能发送一条arp报文问问谁拥有这个IP地址了
    if (arp_send_request_ethii_ipv4(pstNetif, unSrcIPAddr, unDstArpIPAddr, penErr) < 0)
        return -1;
    return 1; 
}

//* 发送地址请求报文
INT arp_send_request_ethii_ipv4(PST_NETIF pstNetif, UINT unSrcIPAddr, UINT unDstArpIPAddr, EN_ONPSERR *penErr)
{
    ST_ETHIIARP_IPV4 stArpRequest; 
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra; 

    //* 封装arp查询请求报文
    stArpRequest.stHdr.usHardwareType = htons(ARP_HARDWARE_ETH);
    stArpRequest.stHdr.usProtoType = htons(ARP_PROTO_IPv4); 
    stArpRequest.stHdr.ubHardwareAddrLen = ETH_MAC_ADDR_LEN;
    stArpRequest.stHdr.ubProtoAddrLen = 4; 
    stArpRequest.stHdr.usOptCode = htons(ARPOPCODE_REQUEST); 
    memcpy(stArpRequest.ubaSrcMacAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN);
    stArpRequest.unSrcIPAddr = unSrcIPAddr; 
    memset(stArpRequest.ubaDstMacAddr, 0, ETH_MAC_ADDR_LEN);  //* 填充全0
    stArpRequest.unDstIPAddr = unDstArpIPAddr; 
    stArpRequest.ubaPadding[0] = 'N';
    stArpRequest.ubaPadding[1] = 'e';
    stArpRequest.ubaPadding[2] = 'o';
    stArpRequest.ubaPadding[3] = '-';
    stArpRequest.ubaPadding[4] = 'T';
    stArpRequest.ubaPadding[5] = 0;

    //* 挂载用户数据
    SHORT sBufListHead = -1;
    SHORT sArpPacketNode = buf_list_get_ext((UCHAR *)&stArpRequest, (UINT)sizeof(stArpRequest), penErr);
    if (sArpPacketNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sArpPacketNode); 

    //* 发送查询报文
    INT nRtnVal = pstNetif->pfunSend(pstNetif, ARP, sBufListHead, "\xFF\xFF\xFF\xFF\xFF\xFF", penErr); 

    //* 释放刚才申请的buf list节点
    buf_list_free(sArpPacketNode); 

    return nRtnVal; 
}

//* 回馈一个reply报文，告知发送者“我”的mac
void arp_send_reply_ethii_ipv4(PST_NETIF pstNetif, UINT unReqIPAddr, UCHAR ubaDstMacAddr[ETH_MAC_ADDR_LEN], UINT unDstArpIPAddr)
{
    EN_ONPSERR enErr;
    ST_ETHIIARP_IPV4 stArpReply;
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra; 

    //* 封装arp查询请求报文
    stArpReply.stHdr.usHardwareType = htons(ARP_HARDWARE_ETH);
    stArpReply.stHdr.usProtoType = htons(ARP_PROTO_IPv4);
    stArpReply.stHdr.ubHardwareAddrLen = ETH_MAC_ADDR_LEN;
    stArpReply.stHdr.ubProtoAddrLen = 4;
    stArpReply.stHdr.usOptCode = htons(ARPOPCODE_REPLY);
    memcpy(stArpReply.ubaSrcMacAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN);
    stArpReply.unSrcIPAddr = unReqIPAddr;
    memcpy(stArpReply.ubaDstMacAddr, ubaDstMacAddr, ETH_MAC_ADDR_LEN);
    stArpReply.unDstIPAddr = unDstArpIPAddr;
    stArpReply.ubaPadding[0] = 'N';
    stArpReply.ubaPadding[1] = 'e';
    stArpReply.ubaPadding[2] = 'o';
    stArpReply.ubaPadding[3] = '-';
    stArpReply.ubaPadding[4] = 'T';
    stArpReply.ubaPadding[5] = 0;

    //* 挂载用户数据    
    SHORT sBufListHead = -1;
    SHORT sArpPacketNode = buf_list_get_ext((UCHAR *)&stArpReply, (UINT)sizeof(stArpReply), &enErr);
    if (sArpPacketNode < 0)
    { 
#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("error: arp_send_reply_ethii_ipv4() failed, %s\r\n", onps_error(enErr)); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }        
    buf_list_put_head(&sBufListHead, sArpPacketNode);

    //* 发送响应报文
    if (pstNetif->pfunSend(pstNetif, ARP, sBufListHead, ubaDstMacAddr, &enErr) < 0)
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("error: arp_send_reply_ethii_ipv4() failed, %s\r\n", onps_error(enErr));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }

    //* 释放刚才申请的buf list节点
    buf_list_free(sArpPacketNode);
}

static BOOL is_arp_broadcast_addr(const UCHAR *pubaMacAddr)
{
    INT i;
    for (i = 0; i < ETH_MAC_ADDR_LEN; i++)
    {
        if (pubaMacAddr[i] != 0x00)
            return FALSE;
    }

    return TRUE;
}

//* 解析并处理ethernet ii层收到的报文
void arp_recv_from_ethii(PST_NETIF pstNetif, UCHAR *pubPacket, INT nPacketLen)
{
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
    PST_ARP_HDR pstHdr = (PST_ARP_HDR)pubPacket; 

    //* arp报文携带的硬件类型是否被协议栈支持
    if (ARP_HARDWARE_ETH != htons(pstHdr->usHardwareType) || ARP_HARDWARE_ADDR_LEN != pstHdr->ubHardwareAddrLen)
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("error: Unsupported arp hardware type (%04X) or mac address length (%d), the packet will be dropped\r\n", htons(pstHdr->usHardwareType), pstHdr->ubHardwareAddrLen); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return; 
    }

    //* arp报文携带的协议类型是否被协议栈支持
    if (ARP_PROTO_IPv4 != htons(pstHdr->usProtoType) || ARP_PROTO_IPv4_ADDR_LEN != pstHdr->ubProtoAddrLen) 
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("error: Unsupported arp protocol type (%04X) or protocol address length (%d), the packet will be dropped\r\n", htons(pstHdr->usProtoType), pstHdr->ubProtoAddrLen); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }

    //* 本协议栈暂时只支持Ipv4版本的arp地址查询
    PST_ETHIIARP_IPV4 pstArpIPv4 = (PST_ETHIIARP_IPV4)pubPacket; 

    //* 既不是广播地址，也不匹配本ethernet网卡mac地址，则直接丢弃该报文
    if (!is_arp_broadcast_addr(pstArpIPv4->ubaDstMacAddr) && !ethernet_mac_matched(pstArpIPv4->ubaDstMacAddr, pstExtra->ubaMacAddr))
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("error: The arp target mac address does not match, the packet will be dropped\r\n"); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }

    //* 进入arp requet和arp reply等业务逻辑
    EN_ARPOPCODE enOpcode = (EN_ARPOPCODE)htons(pstHdr->usOptCode); 
    switch (enOpcode)
    {
    case ARPOPCODE_REQUEST: 
        //* 如果目标ip地址匹配则回馈一个arp reply报文给发送者
        if (ethernet_ipv4_addr_matched(pstNetif, pstArpIPv4->unDstIPAddr))
            arp_send_reply_ethii_ipv4(pstNetif, pstArpIPv4->unDstIPAddr, pstArpIPv4->ubaSrcMacAddr, pstArpIPv4->unSrcIPAddr);
        break; 

    case ARPOPCODE_REPLY:
        //* 确定目标ip地址与本网卡绑定的ip地址匹配，只有匹配才会将其加入arp缓存表
        if (ethernet_ipv4_addr_matched(pstNetif, pstArpIPv4->unDstIPAddr))
            arp_add_ethii_ipv4(pstExtra->pstcbArp->staEntryIPv4, pstArpIPv4->unSrcIPAddr, pstArpIPv4->ubaSrcMacAddr); 
        break; 

    default:  // 不做任何处理
        break; 
    }
}

#endif

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
#include "ethernet/arp.h" 
#include "ethernet/ethernet_frame.h"
#define SYMBOL_GLOBALS
#include "ethernet/ethernet.h"
#undef SYMBOL_GLOBALS

//* 保存ethernet网卡附加信息的静态存储时期变量，系统存在几个ethernet网卡，这里就会申请几个数组单元
static ST_NETIFEXTRA_ETH l_staExtraOfEth[ETHERNET_NUM]; 

void ethernet_init(void)
{
    arp_init(); 
    memset(l_staExtraOfEth, 0, sizeof(l_staExtraOfEth)); 
}

//* 添加ethernet网卡
PST_NETIF ethernet_add(const CHAR *pszIfName, const UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], PST_IPV4 pstIPv4, PFUN_EMAC_SEND pfunEmacSend, EN_ONPSERR *penErr)
{
    PST_NETIF pstNetif = NULL; 
    PST_NETIFEXTRA_ETH pstExtra = NULL; 
    INT i; 
    os_critical_init(); 
    os_enter_critical();
    {
        for (i = 0; i < ETHERNET_NUM; i++)
        {
            if (!l_staExtraOfEth[i].bIsUsed)
            {
                pstExtra = &l_staExtraOfEth[i]; 
                pstExtra->bIsUsed = TRUE;                  
                break; 
            }
        }
    }
    os_exit_critical();    

    if (!pstExtra)
    {
        if (penErr)
            *penErr = ERRETHEXTRAEMPTY;
        return NULL;
    }

    //* 申请一个arp控制块
    PSTCB_ETHARP pstcbArp = arp_ctl_block_new(); 
    if (!pstcbArp)
    {
        pstExtra->bIsUsed = FALSE; //* 归还
        if (penErr)
            *penErr = ERRNEWARPCTLBLOCK;
        return NULL;
    }

    PST_NETIF_NODE pstIfNode = netif_add(NIF_ETHERNET, pszIfName, pstIPv4, ethernet_ii_send, pstExtra, penErr);
    if (pstIfNode)
    {
        pstNetif = &pstIfNode->stIf;

        pstExtra->pstIPList = NULL;
        pstExtra->pstcbArp = pstcbArp; 
        pstExtra->pfunEmacSend = pfunEmacSend; 
        memcpy(pstExtra->ubaMacAddr, ubaMacAddr, ETH_MAC_ADDR_LEN);
        if (pstIPv4->unAddr) //* 地址不为0则为静态地址，需要将其添加到路由表中
        {
            pstExtra->bIsStaticAddr = TRUE;

            //* 添加到路由表，使其成为缺省路由
            if (!route_add(pstNetif, 0, 0, 0, penErr))
            {
                netif_del(pstIfNode);
                pstNetif = NULL; 
                
                arp_ctl_block_free(pstcbArp);   //* 释放arp控制块
                pstExtra->bIsUsed = FALSE;      //* 归还刚刚占用的附加信息节点，不需要关中断进行保护，获取节点的时候需要
            }
        }
        else        
            pstExtra->bIsStaticAddr = FALSE;         
    }

    return pstNetif;
}

//* 删除ethernet网卡
void ethernet_del(PST_NETIF pstNetif)
{
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra; 

    //* 释放占用的arp控制块
    arp_ctl_block_free(pstExtra->pstcbArp); 
    pstExtra->bIsUsed = FALSE; 

    //* 先从路由表删除
    route_del_ext(pstNetif); 

    //* 再从网卡链表删除
    netif_del_ext(pstNetif); 
}

//* 通过ethernet网卡进行发送
INT ethernet_ii_send(PST_NETIF pstNetif, UCHAR ubProtocol, SHORT sBufListHead, void *pvExtraParam, EN_ONPSERR *penErr)
{
    ST_ETHERNET_II_HDR stEthIIHdr; 
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;

    //* 填充源与目标mac地址
    memcpy(stEthIIHdr.ubaDstMacAddr, (const UCHAR *)pvExtraParam, ETH_MAC_ADDR_LEN); 
    memcpy(stEthIIHdr.ubaSrcMacAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN); 

    //* 转换成ethernte ii定义的协议类型值
    switch((EN_NPSPROTOCOL)ubProtocol)
    {
    case IPV4: 
        stEthIIHdr.usProtoType = htons(ETHII_IP);
        break; 

    case ARP: 
        stEthIIHdr.usProtoType = htons(ETHII_ARP);
        break; 

    default: 
        if (penErr)
            *penErr = ERRUNSUPPETHIIPROTO; 
        return -1; 
    }

    //* ethernet ii协议头挂载到链表头部
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stEthIIHdr, (USHORT)sizeof(ST_ETHERNET_II_HDR), penErr);
    if (sHdrNode < 0)
    {
        //* 使用计数减一，释放对网卡资源的占用
        netif_freed(pstNetif);
        return -1;
    }
    buf_list_put_head(&sBufListHead, sHdrNode);

    //* 发送数据
    INT nRtnVal = pstExtra->pfunEmacSend(sBufListHead, penErr); 

    //* 释放刚才申请的buf list节点
    buf_list_free(sHdrNode); 

    return nRtnVal; 
}

//* 处理ethernet网卡收到的ethernet ii协议帧
void ethernet_ii_recv(PST_NETIF pstNetif, UCHAR *pubPacket, INT nPacketLen)
{
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;

    if (nPacketLen < (INT)sizeof(ST_ETHERNET_II_HDR))
        return;     

    PST_ETHERNET_II_HDR pstHdr = (PST_ETHERNET_II_HDR)pubPacket; 

    //* 既不是广播地址，也不匹配本ethernet网卡mac地址，则直接丢弃该报文
    if (!is_mac_broadcast_addr(pstHdr->ubaDstMacAddr) && !ethernet_mac_matched(pstHdr->ubaDstMacAddr, pstExtra->ubaMacAddr))
        return; 

    //* 根据ethernet ii帧携带的协议类型分别处理之
    USHORT usProtocolType = htons(pstHdr->usProtoType);
    switch (usProtocolType)
    {
    case ETHII_IP: 
        ip_recv(pubPacket + sizeof(ST_ETHERNET_II_HDR), nPacketLen - (INT)sizeof(ST_ETHERNET_II_HDR));
        break; 

    case ETHII_ARP: 
        arp_recv_from_ethii(pstNetif, pubPacket + sizeof(ST_ETHERNET_II_HDR), nPacketLen - (INT)sizeof(ST_ETHERNET_II_HDR)); 
        break; 

#if SUPPORT_IPV6
    case ETHII_IPV6:
        break; 
#endif

    default: 
#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("error: Unsupported ethernet ii protocol type (%04X), the packet will be discarded\r\n", usProtocolType);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        break; 
    }
}

//* 参数unTargetIpAddr指定的ip地址是否与ethernet网卡的ip地址匹配
BOOL ethernet_ipv4_addr_matched(PST_NETIF pstNetif, in_addr_t unTargetIpAddr)
{
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra; 

    if (unTargetIpAddr == pstNetif->stIPv4.unAddr)
        return TRUE; 

    //* 看看附加ip地址列表有匹配的吗
    PST_NETIF_ETH_IP_NODE pstNextIpNode = pstExtra->pstIPList; 
    while (pstNextIpNode)
    {
        if (unTargetIpAddr == pstNextIpNode->unAddr)
            return TRUE; 

        pstNextIpNode = pstNextIpNode->pstNext; 
    }

    return FALSE; 
}

#endif

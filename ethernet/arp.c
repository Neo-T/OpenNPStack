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
void arp_add_ethii_ipv4(PST_ENTRY_ETHIIIPV4 pstArpIPv4Tbl, UINT unIPAddr, UCHAR ubaMacAddr[6])
{
    INT i; 

    os_critical_init(); 

    //* 先查找该条目是否已经存在，如果存在则直接更新，否则直接添加或替换最老条目
    for (i = 0; i < ARPENTRY_NUM; i++)
    {
        //* 尚未缓存任何条目，不必继续查找了，直接新增即可
        if (!pstArpIPv4Tbl[i].unIPAddr)
            break;         

        if (unIPAddr == pstArpIPv4Tbl[i].unIPAddr) //* 匹配
        {
            //* 更新mac地址
            os_enter_critical();
            {
                memcpy(pstArpIPv4Tbl[i].ubaMacAddr, ubaMacAddr, 6);
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
        memcpy(pstArpIPv4Tbl[i].ubaMacAddr, ubaMacAddr, sizeof(ubaMacAddr));
        pstArpIPv4Tbl[i].unUpdateTime = os_get_system_secs();
        pstArpIPv4Tbl[i].unIPAddr = unIPAddr;
    }
    os_exit_critical();
}

INT arp_get_mac(PST_NETIF pstNetif, UINT unIPAddr, UCHAR ubaMacAddr[6], EN_ONPSERR *penErr)
{
    PSTCB_ETHARP pstcbArp = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbArp; 

    os_critical_init(); 

    //* 是否命中最近刚读取过的条目
    os_enter_critical();
    if (unIPAddr == pstcbArp->staEntryIPv4[pstcbArp->bLastEntryIPv4ToRead].unIPAddr) 
    {        
        memcpy(ubaMacAddr, pstcbArp->staEntryIPv4[pstcbArp->bLastEntryIPv4ToRead].ubaMacAddr, 6);
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
        if (unIPAddr == pstcbArp->staEntryIPv4[i].unIPAddr)
        {
            memcpy(ubaMacAddr, pstcbArp->staEntryIPv4[i].ubaMacAddr, 6);
            pstcbArp->staEntryIPv4[i].unUpdateTime = os_get_system_secs();
            pstcbArp->bLastEntryIPv4ToRead = i;
            os_exit_critical();
            return 0;
        }
        os_exit_critical();
    }

    //* 不存在，则只能发送一条arp报文问问谁拥有这个IP地址了
    if (arp_send_request_ethii_ipv4(unIPAddr, penErr) < 0)
        return -1;
    return 1; 
}

//* 发送地址请求报文
INT arp_send_request_ethii_ipv4(PST_NETIF pstNetif, UINT unIPAddr, EN_ONPSERR *penErr)
{
    ST_ETHIIARP_IPV4 stArpRequest; 

    //* 首先确定这个需要走哪个ethernet网卡，如果为空，说明目标地址非法直接报错
    PST_NETIF pstNetif = netif_get_by_genmask(unIPAddr); 
    if (!pstNetif)
    {
        if (penErr)
            *penErr = ERRNETUNREACHABLE;
        return -1; 
    }

    stArpRequest.stHdr.usHardwareType = htons(ARP_HARDWARE_ETH);
    stArpRequest.stHdr.usProtoType = htons(ARP_PROTO_IPv4); 
    stArpRequest.stHdr.ubHardwareAddrLen = 6; 
    stArpRequest.stHdr.ubProtoAddrLen = 4; 
    stArpRequest.stHdr.usOptCode = ARPOPCODE_REQUEST; 
}

#endif

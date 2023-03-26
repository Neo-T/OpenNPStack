/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 完成arp模块相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.06.16 13:43
 *
 */
#ifndef ARP_H
#define ARP_H

#ifdef SYMBOL_GLOBALS
	#define ARP_EXT
#else
	#define ARP_EXT extern
#endif //* SYMBOL_GLOBALS

#if SUPPORT_ETHERNET
//* arp条目表
typedef struct _ST_ENTRY_ETHIIIPV4_ {
    UINT unUpdateTime;      //* 条目更新（读取/缓存）时间
    UINT unIPAddr;          //* IP地址
    UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN]; //* 对应的ip地址    
} ST_ENTRY_ETHIIIPV4, *PST_ENTRY_ETHIIIPV4;

//* arp条目控制块
typedef struct _STCB_ETHARP_ {
    CHAR bIsUsed;
    CHAR bLastReadEntryIdx; //* 最近读取的arp条目索引
    ST_ENTRY_ETHIIIPV4 staEntry[ARPENTRY_NUM]; //* arp条目缓存表（Ipv4地址到以太网mac地址映射表）

    ST_SLINKEDLIST_NODE staSListWaitQueue[12];		//* 等待arp查询结果的待发送报文队列    
	PST_SLINKEDLIST pstSListWaitQueueFreed; 
    PST_SLINKEDLIST pstSListWaitQueue; 
} STCB_ETHARP, *PSTCB_ETHARP;

//* 等待arp查询结束后重新发送ip报文的控制块
typedef struct _STCB_ETH_ARP_WAIT_ {
    //PST_ONESHOTTIMER pstTimer; 
    PST_NETIF pstNetif;
    PST_SLINKEDLIST_NODE pstNode; 
	UINT unIpv4;
    USHORT usIpPacketLen;
    UCHAR ubCount;
	UCHAR ubSndStatus; 
} STCB_ETH_ARP_WAIT, *PSTCB_ETH_ARP_WAIT;

ARP_EXT void arp_init(void); 
ARP_EXT PSTCB_ETHARP arp_ctl_block_new(void);
ARP_EXT void arp_ctl_block_free(PSTCB_ETHARP pstcbArp);
ARP_EXT void arp_add_ethii_ipv4(PST_NETIF pstNetif/*PST_ENTRY_ETHIIIPV4 pstArpIPv4Tbl*/, UINT unIPAddr, UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN]);
ARP_EXT void arp_add_ethii_ipv4_ext(PST_ENTRY_ETHIIIPV4 pstArpIPv4Tbl, UINT unIPAddr, UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN]); 
ARP_EXT INT arp_get_mac(PST_NETIF pstNetif, UINT unSrcIPAddr, UINT unDstArpIPAddr, UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], EN_ONPSERR *penErr);
ARP_EXT INT arp_get_mac_ext(PST_NETIF pstNetif, UINT unSrcIPAddr, UINT unDstArpIPAddr, UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], SHORT sBufListHead, BOOL *pblNetifFreedEn, EN_ONPSERR *penErr);
ARP_EXT INT arp_send_request_ethii_ipv4(PST_NETIF pstNetif, UINT unSrcIPAddr, UINT unDstArpIPAddr, EN_ONPSERR *penErr);
ARP_EXT void arp_recv_from_ethii(PST_NETIF pstNetif, UCHAR *pubPacket, INT nPacketLen); 
#endif

#endif

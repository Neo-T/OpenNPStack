/* arp.h
 *
 * 完成arp模块相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.06.16 13:43
 * 版本: 1.0
 *
 */
#ifndef ARP_H
#define ARP_H

#ifdef SYMBOL_GLOBALS
	#define ARP_EXT
#else
	#define ARP_EXT extern
#endif //* SYMBOL_GLOBALS

 //* arp条目表
typedef struct _ST_ENTRY_ETHIIIPV4_ {
    UINT unUpdateTime;      //* arp条目更新（读取/缓存）时间
    UINT unIPAddr;          //* IP地址
    UCHAR ubaMacAddr[6];    //* 对应的ip地址    
} ST_ENTRY_ETHIIIPV4, *PST_ENTRY_ETHIIIPV4;

//* arp条目控制块
typedef struct _STCB_ETHARP_ {
    CHAR bIsUsed;
    CHAR bLastEntryIPv4ToRead; //* 最近读取的arp条目
    ST_ENTRY_ETHIIIPV4 staEntryIPv4[ARPENTRY_NUM]; //* arp条目缓存表
} STCB_ETHARP, *PSTCB_ETHARP;

ARP_EXT void arp_init(void); 
ARP_EXT PSTCB_ETHARP arp_ctl_block_new(void);
ARP_EXT void arp_ctl_block_free(PSTCB_ETHARP pstcbArp);
ARP_EXT void arp_add_ethii_ipv4(PST_ENTRY_ETHIIIPV4 pstArpIPv4Tbl, UINT unIPAddr, UCHAR ubaMacAddr[6]);
ARP_EXT INT arp_get_mac(PST_NETIF pstNetif, UINT unIPAddr, UCHAR ubaMacAddr[6], EN_ONPSERR *penErr);
ARP_EXT INT arp_send_request_ethii_ipv4(PST_NETIF pstNetif, UINT unIPAddr, EN_ONPSERR *penErr);

#endif

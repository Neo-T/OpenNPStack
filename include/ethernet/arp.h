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

ARP_EXT void arp_init(void); 
ARP_EXT void arp_add_ethii_ipv4(UINT unIPAddr, UCHAR ubaMacAddr[6]); 
ARP_EXT INT arp_get_mac(UINT unIPAddr, UCHAR ubaMacAddr[6], EN_ONPSERR *penErr);
ARP_EXT INT arp_send_request_ethii_ipv4(UINT unIPAddr, EN_ONPSERR *penErr);

#endif
